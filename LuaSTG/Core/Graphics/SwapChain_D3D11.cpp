﻿#include "Core/Graphics/SwapChain_D3D11.hpp"
#include "Core/Graphics/Format_D3D11.hpp"
#include "Core/i18n.hpp"
#include "utility/encoding.hpp"
#include "platform/WindowsVersion.hpp"
#include "Platform/DesktopWindowManager.hpp"

#include "ScreenGrab11.h"

//#define _log(x) OutputDebugStringA(x "\n")
#define _log(x)

#define HRNew HRESULT hr = S_OK;
#define HRGet hr = gHR
#define HRCheckCallReport(x) if (FAILED(hr)) { i18n_core_system_call_report_error(x); }
#define HRCheckCallReturnBool(x) if (FAILED(hr)) { i18n_core_system_call_report_error(x); assert(false); return false; }
#define HRCheckCallNoAssertReturnBool(x) if (FAILED(hr)) { i18n_core_system_call_report_error(x); return false; }

namespace Core::Graphics
{
	constexpr DXGI_FORMAT const COLOR_BUFFER_FORMAT = DXGI_FORMAT_B8G8R8A8_UNORM;
	constexpr DXGI_FORMAT const DEPTH_BUFFER_FORMAT = DXGI_FORMAT_D24_UNORM_S8_UINT;

	constexpr uint32_t const BACKGROUND_W = 512; // 512 * 16 = 8192 一般显示器大概也超不过这个分辨率？
	constexpr uint32_t const BACKGROUND_H = 512; // 16x 是硬件支持的最大放大级别 0.25x 是最小缩小级别，128 ~ 8192

	inline bool compare_DXGI_MODE_DESC_main(DXGI_MODE_DESC const& a, DXGI_MODE_DESC const& b)
	{
		return a.Width == b.Width
			&& a.Height == b.Height
			&& a.RefreshRate.Numerator == b.RefreshRate.Numerator
			&& a.RefreshRate.Denominator == b.RefreshRate.Denominator
			&& a.Format == b.Format
			;
	}
	inline bool is_DXGI_MODE_equal(DXGI_MODE_DESC const& a, DXGI_MODE_DESC const& b)
	{
		return a.Width == b.Width
			&& a.Height == b.Height
			&& a.RefreshRate.Numerator == b.RefreshRate.Numerator
			&& a.RefreshRate.Denominator == b.RefreshRate.Denominator
			&& a.Format == b.Format
			&& a.ScanlineOrdering == b.ScanlineOrdering
			&& a.Scaling == b.Scaling
			;
	}
	inline bool is_DXGI_MODE_better(DXGI_MODE_DESC const& a, DXGI_MODE_DESC const& b)
	{
		if (a.ScanlineOrdering != b.ScanlineOrdering && a.Scaling != b.Scaling)
		{
			if (b.ScanlineOrdering == DXGI_MODE_SCANLINE_ORDER_PROGRESSIVE && b.Scaling == DXGI_MODE_SCALING_UNSPECIFIED)
				return true; // 逐行扫描 + 自动缩放 更好
		}
		else if (a.ScanlineOrdering != b.ScanlineOrdering)
		{
			if (b.ScanlineOrdering == DXGI_MODE_SCANLINE_ORDER_PROGRESSIVE)
				return true; // 逐行扫描 更好
		}
		else if (a.Scaling != b.Scaling)
		{
			if (b.Scaling == DXGI_MODE_SCALING_UNSPECIFIED)
				return true; // 自动缩放 更好
		}
		return false; // 没有更好的点
	}
	inline bool makeLetterboxing(Vector2U rect, Vector2U inner_rect, DXGI_MATRIX_3X2_F& mat)
	{
		if (rect == inner_rect)
		{
			return false; // 不需要
		}
		else
		{
			double const hscale = (double)rect.x / (double)inner_rect.x;
			double const vscale = (double)rect.y / (double)inner_rect.y;
			double const scale = std::min(hscale, vscale);
			double const width = scale * (double)inner_rect.x;
			double const height = scale * (double)inner_rect.y;
			double const x = ((double)rect.x - width) * 0.5;
			double const y = ((double)rect.y - height) * 0.5;
			mat = DXGI_MATRIX_3X2_F{
				FLOAT(scale), 0.0f,
				0.0f, FLOAT(scale),
				FLOAT(x), FLOAT(y),
			};
			return true;
		}
	}
	inline DXGI_SWAP_CHAIN_DESC1 getDefaultSwapChainInfo7()
	{
		return DXGI_SWAP_CHAIN_DESC1{
			.Width = 0,
			.Height = 0,
			.Format = COLOR_BUFFER_FORMAT,
			.Stereo = FALSE,
			.SampleDesc = DXGI_SAMPLE_DESC{
				.Count = 1,
				.Quality = 0,
			},
			.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
			.BufferCount = 1,
			.Scaling = DXGI_SCALING_STRETCH,
			.SwapEffect = DXGI_SWAP_EFFECT_DISCARD,
			.AlphaMode = DXGI_ALPHA_MODE_IGNORE,
			.Flags = 0,
		};
	}
	inline DXGI_SWAP_CHAIN_DESC1 getDefaultSwapChainInfo10()
	{
		return DXGI_SWAP_CHAIN_DESC1{
			.Width = 0,
			.Height = 0,
			.Format = COLOR_BUFFER_FORMAT,
			.Stereo = FALSE,
			.SampleDesc = DXGI_SAMPLE_DESC{
				.Count = 1,
				.Quality = 0,
			},
			.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
			.BufferCount = 2,
			.Scaling = DXGI_SCALING_NONE,
			.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD,
			.AlphaMode = DXGI_ALPHA_MODE_IGNORE,
			.Flags = 0,
		};
	}
	static bool findBestDisplayMode(IDXGISwapChain1* dxgi_swapchain, Vector2U canvas_size, DXGI_MODE_DESC1& mode)
	{
		HRNew;

		Microsoft::WRL::ComPtr<IDXGIOutput> dxgi_output;
		HRGet = dxgi_swapchain->GetContainingOutput(&dxgi_output);
		HRCheckCallNoAssertReturnBool("IDXGISwapChain1::GetContainingOutput");

		Microsoft::WRL::ComPtr<IDXGIOutput1> dxgi_output_1;
		HRGet = dxgi_output.As(&dxgi_output_1);
		HRCheckCallReturnBool("IDXGIOutput::QueryInterface -> IDXGIOutput1");

		DXGI_OUTPUT_DESC dxgi_output_info = {};
		HRGet = dxgi_output_1->GetDesc(&dxgi_output_info);
		HRCheckCallReturnBool("IDXGIOutput1::GetDesc");
		assert(dxgi_output_info.AttachedToDesktop);
		assert(dxgi_output_info.Monitor);
		assert(dxgi_output_info.DesktopCoordinates.right > dxgi_output_info.DesktopCoordinates.left);
		assert(dxgi_output_info.DesktopCoordinates.bottom > dxgi_output_info.DesktopCoordinates.top);
		UINT const monitor_w = static_cast<UINT>(dxgi_output_info.DesktopCoordinates.right - dxgi_output_info.DesktopCoordinates.left);
		UINT const monitor_h = static_cast<UINT>(dxgi_output_info.DesktopCoordinates.bottom - dxgi_output_info.DesktopCoordinates.top);

		UINT dxgi_mode_count = 0;
		HRGet = dxgi_output_1->GetDisplayModeList1(COLOR_BUFFER_FORMAT, 0, &dxgi_mode_count, NULL);
		HRCheckCallReturnBool("IDXGIOutput1::GetDisplayModeList1 -> DXGI_FORMAT_B8G8R8A8_UNORM");
		assert(dxgi_mode_count > 0);

		std::vector<DXGI_MODE_DESC1> mode_list(dxgi_mode_count);
		HRGet = dxgi_output_1->GetDisplayModeList1(COLOR_BUFFER_FORMAT, 0, &dxgi_mode_count, mode_list.data());
		HRCheckCallReturnBool("IDXGIOutput1::GetDisplayModeList1 -> DXGI_FORMAT_B8G8R8A8_UNORM");

		// 公共函数

		auto compareRefreshRate = [](DXGI_MODE_DESC1 const& a, DXGI_MODE_DESC1 const& b) -> bool
		{
			double const hz_a = (double)a.RefreshRate.Numerator / (double)a.RefreshRate.Denominator;
			double const hz_b = (double)b.RefreshRate.Numerator / (double)b.RefreshRate.Denominator;
			return hz_a > hz_b;
		};

		auto checkRequiredRefreshRate = [](DXGI_MODE_DESC1 const& v) -> bool
		{
			return (double)v.RefreshRate.Numerator / (double)v.RefreshRate.Denominator >= 59.5;
		};

		auto checkRequiredCanvasSize = [&](DXGI_MODE_DESC1 const& v) -> bool
		{
			return v.Width >= canvas_size.x && v.Height >= canvas_size.y;
		};

		auto checkRequiredAspectRatio = [&](DXGI_MODE_DESC1 const& v) -> bool
		{
			if (monitor_w > monitor_h)
			{
				UINT const width = v.Height * monitor_w / monitor_h;
				assert(width > 0);
				if (width == 0) return false;
				else if (v.Width == width) return true;
				else if (width > v.Width && (width - v.Width) <= 2) return true;
				else if (v.Width > width && (v.Width - width) <= 2) return true;
				else return false;
			}
			else
			{
				UINT const height = v.Width * monitor_h / monitor_w;
				assert(height > 0);
				if (height == 0) return false;
				else if (v.Height == height) return true;
				else if (height > v.Height && (height - v.Height) <= 2) return true;
				else if (v.Height > height && (v.Height - height) <= 2) return true;
				else return false;
			}
		};

		// 默认的桌面分辨率

		std::vector<DXGI_MODE_DESC1> default_mode_list;
		for (auto const& v : mode_list)
		{
			if (v.Width == monitor_w && v.Height == monitor_h)
			{
				default_mode_list.emplace_back(v);
			}
		}

		assert(!default_mode_list.empty());
		if (default_mode_list.empty()) return false;

		std::sort(default_mode_list.begin(), default_mode_list.end(), compareRefreshRate);
		DXGI_MODE_DESC1 default_mode = default_mode_list.at(0);
		default_mode_list.clear();

		// 剔除隔行扫描（理论情况下不会出现）

		for (auto it = mode_list.begin(); it != mode_list.end();)
		{
			if (!(it->ScanlineOrdering == DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED || it->ScanlineOrdering == DXGI_MODE_SCANLINE_ORDER_PROGRESSIVE))
				it = mode_list.erase(it);
			else
				it++;
		}

		// 剔除刷新率过低的

		for (auto it = mode_list.begin(); it != mode_list.end();)
		{
			if (!checkRequiredRefreshRate(*it))
			{
				it = mode_list.erase(it);
			}
			else
			{
				it++;
			}
		}

		// 剔除分辨率比画布分辨率低的

		for (auto it = mode_list.begin(); it != mode_list.end();)
		{
			if (!checkRequiredCanvasSize(*it))
			{
				it = mode_list.erase(it);
			}
			else
			{
				it++;
			}
		}

		// 剔除横纵比不和显示器匹配的

		for (auto it = mode_list.begin(); it != mode_list.end();)
		{
			if (!checkRequiredAspectRatio(*it))
			{
				it = mode_list.erase(it);
			}
			else
			{
				it++;
			}
		}

		// 根据面积比和刷新率进行排序，优先刷新率，然后是面积比

		auto compareByRefreshRateAndSize = [&](DXGI_MODE_DESC1 const& a, DXGI_MODE_DESC1 const& b) -> bool
		{
			double const canvas_sz = (double)(canvas_size.x * canvas_size.y);
			double const sz_a = canvas_sz / (double)(a.Width * a.Height);
			double const sz_b = canvas_sz / (double)(b.Width * b.Height);
			if (sz_a == sz_b)
			{
				return compareRefreshRate(a, b);
			}
			else
			{
				return sz_b > sz_b;
			}
		};

		std::sort(mode_list.begin(), mode_list.end(), compareByRefreshRateAndSize);

		// 打印结果

		i18n_log_info_fmt("[core].SwapChain_D3D11.found_N_DisplayMode_fmt", mode_list.size());
		for (size_t i = 0; i < mode_list.size(); i += 1)
		{
			spdlog::info("{: >4d}: ({: >5d} x {: >5d}) {:.2f}Hz"
				, i
				, mode_list[i].Width, mode_list[i].Height
				, (double)mode_list[i].RefreshRate.Numerator / (double)mode_list[i].RefreshRate.Denominator
			);
		}

		// 最终，挑选出面积比最大且刷新率最高的

		if (!mode_list.empty())
		{
			mode = mode_list.at(0);
		}
		else
		{
			mode = default_mode;
		}

		return true;
	}
	static bool checkHardwareCompositionSupport(ID3D11Device* device)
	{
		assert(device);

		HRNew;

		// 不是 Windows 10 则跳过

		if (!IsWindows10OrGreater())
		{
			return false;
		}

		// 不支持画面撕裂则跳过

		Microsoft::WRL::ComPtr<IDXGIFactory2> dxgi_factory;
		Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device = device;

		HRGet = Platform::RuntimeLoader::Direct3D11::GetFactory(d3d11_device.Get(), &dxgi_factory);
		HRCheckCallReturnBool("ID3D11Device::GetParent -> IDXGIFactory2");

		if (!Platform::RuntimeLoader::DXGI::CheckFeatureSupportPresentAllowTearing(dxgi_factory.Get()))
		{
			return false;
		}

		// 检查显示输出拓扑

		bool v_primary_support = true;
		bool v_support = true;

		Microsoft::WRL::ComPtr<IDXGIAdapter1> v_adapter;
		for (UINT i_adapter = 0; SUCCEEDED(dxgi_factory->EnumAdapters1(i_adapter, &v_adapter)); i_adapter += 1)
		{
			Microsoft::WRL::ComPtr<IDXGIOutput> v_output;
			for (UINT i_output = 0; SUCCEEDED(v_adapter->EnumOutputs(i_output, &v_output)); i_output += 1)
			{
				BOOL is_primary = FALSE;
				BOOL overlays = FALSE;
				UINT overlay_support = 0;
				UINT hardware_composition_support = 0;

				DXGI_OUTPUT_DESC v_output_info = {};
				HRGet = v_output->GetDesc(&v_output_info);
				HRCheckCallReport("IDXGIOutput::GetDesc");
				if (SUCCEEDED(hr))
				{
					MONITORINFOEXW v_monitor_info = {};
					v_monitor_info.cbSize = sizeof(v_monitor_info);
					if (!GetMonitorInfoW(v_output_info.Monitor, &v_monitor_info))
					{
						hr = HRESULT_FROM_WIN32(GetLastError());
						HRCheckCallReport("GetMonitorInfoW");
					}
					else
					{
						if (v_monitor_info.dwFlags & MONITORINFOF_PRIMARY)
						{
							is_primary = TRUE;
						}
					}
				}

				Microsoft::WRL::ComPtr<IDXGIOutput2> v_output2;
				HRGet = v_output.As(&v_output2);
				HRCheckCallReport("IDXGIOutput::QueryInterface -> IDXGIOutput2");
				if (v_output2)
				{
					overlays = v_output2->SupportsOverlays();
				}

				Microsoft::WRL::ComPtr<IDXGIOutput3> v_output3;
				HRGet = v_output.As(&v_output3);
				HRCheckCallReport("IDXGIOutput::QueryInterface -> IDXGIOutput3");
				if (v_output3)
				{
					HRGet = v_output3->CheckOverlaySupport(DXGI_FORMAT_B8G8R8A8_UNORM, d3d11_device.Get(), &overlay_support);
					HRCheckCallReport("IDXGIOutput3::CheckOverlaySupport -> DXGI_FORMAT_B8G8R8A8_UNORM");
				}

				Microsoft::WRL::ComPtr<IDXGIOutput6> v_output6;
				HRGet = v_output.As(&v_output6);
				HRCheckCallReport("IDXGIOutput::QueryInterface -> IDXGIOutput6");
				if (v_output6)
				{
					HRGet = v_output6->CheckHardwareCompositionSupport(&hardware_composition_support);
					HRCheckCallReport("IDXGIOutput6::CheckHardwareCompositionSupport");
				}

				bool const condition1 = (overlays);
				bool const condition2 = (overlay_support & DXGI_OVERLAY_SUPPORT_FLAG_DIRECT) && (overlay_support & DXGI_OVERLAY_SUPPORT_FLAG_SCALING);
				bool const condition3 = (hardware_composition_support & DXGI_HARDWARE_COMPOSITION_SUPPORT_FLAG_FULLSCREEN);
				if (is_primary)
				{
					if (!((condition1 || condition2) && condition3))
					{
						v_primary_support = false;
						v_support = false;
					}
				}
				else
				{
					// 多显示输出系统上，似乎只有主显示输出会报告多平面叠加支持
					if (!((condition1 || condition2 || v_primary_support) && condition3))
					{
						v_support = false;
					}
				}
			}
		}

		return v_support;
	}

	void SwapChain_D3D11::dispatchEvent(EventType t)
	{
		// 回调
		m_is_dispatch_event = true;
		switch (t)
		{
		case EventType::SwapChainCreate:
			for (auto& v : m_eventobj)
			{
				if (v) v->onSwapChainCreate();
			}
			break;
		case EventType::SwapChainDestroy:
			for (auto& v : m_eventobj)
			{
				if (v) v->onSwapChainDestroy();
			}
			break;
		}
		m_is_dispatch_event = false;
		// 处理那些延迟的对象
		removeEventListener(nullptr);
		for (auto& v : m_eventobj_late)
		{
			m_eventobj.emplace_back(v);
		}
		m_eventobj_late.clear();
	}
	void SwapChain_D3D11::addEventListener(ISwapChainEventListener* e)
	{
		removeEventListener(e);
		if (m_is_dispatch_event)
		{
			m_eventobj_late.emplace_back(e);
		}
		else
		{
			m_eventobj.emplace_back(e);
		}
	}
	void SwapChain_D3D11::removeEventListener(ISwapChainEventListener* e)
	{
		if (m_is_dispatch_event)
		{
			for (auto& v : m_eventobj)
			{
				if (v == e)
				{
					v = nullptr; // 不破坏遍历过程
				}
			}
		}
		else
		{
			for (auto it = m_eventobj.begin(); it != m_eventobj.end();)
			{
				if (*it == e)
					it = m_eventobj.erase(it);
				else
					it++;
			}
		}
	}

	void SwapChain_D3D11::onDeviceCreate()
	{
		m_scaling_renderer.AttachDevice(m_device->GetD3D11Device());
		if (m_init) // 曾经设置过
		{
			setWindowMode({ m_swapchain_last_mode.width, m_swapchain_last_mode.height });
			if (m_swapchain_last_windowed)
			{
				m_swapchain_last_windowed = FALSE;
				onWindowFullscreenStateChange(true);
			}
		}
	}
	void SwapChain_D3D11::onDeviceDestroy()
	{
		destroySwapChain();
		m_scaling_renderer.DetachDevice();
	}
	void SwapChain_D3D11::onWindowCreate()
	{
		// 目前窗口的重新创建只会被交换链触发，所以这里留空
	}
	void SwapChain_D3D11::onWindowDestroy()
	{
		// 目前窗口的重新创建只会被交换链触发，所以这里留空
	}
	void SwapChain_D3D11::onWindowActive()
	{
		if (!dxgi_swapchain)
			return;
		_log("onWindowActive");
		m_swapchain_want_present_reset = TRUE;
		enterExclusiveFullscreenTemporarily();
	}
	void SwapChain_D3D11::onWindowInactive()
	{
		if (!dxgi_swapchain)
			return;
		_log("onWindowInactive");
		leaveExclusiveFullscreenTemporarily();
	}
	void SwapChain_D3D11::onWindowSize(Core::Vector2U size)
	{
		if (size.x == 0 || size.y == 0)
			return; // 忽略窗口最小化
		if (!dxgi_swapchain)
			return; // 此时交换链还未初始化
		if (m_is_composition_mode)
		{
			handleDirectCompositionWindowSize(size);
		}
		else
		{
			handleSwapChainWindowSize(size);
		}
	}
	void SwapChain_D3D11::onWindowFullscreenStateChange(bool state)
	{
		if (!dxgi_swapchain)
			return;
		if (state)
			enterExclusiveFullscreen();
		else
			leaveExclusiveFullscreen();
	}

	bool SwapChain_D3D11::createSwapChain(bool windowed, bool flip, bool latency_event, DisplayMode const& mode, bool no_attachment)
	{
		_log("createSwapChain");

		HRESULT hr = 0;

		i18n_log_info("[core].SwapChain_D3D11.start_creating_swapchain");

		// 必须成功的操作

		if (!m_window->GetWindow())
		{
			i18n_log_error("[core].SwapChain_D3D11.create_swapchain_failed_null_window");
			assert(false); return false;
		}
		if (!m_device->GetDXGIFactory2()) // 强制要求平台更新
		{
			i18n_log_error("[core].SwapChain_D3D11.create_swapchain_failed_null_DXGI");
			assert(false); return false;
		}
		if (!m_device->GetD3D11Device())
		{
			i18n_log_error("[core].SwapChain_D3D11.create_swapchain_failed_null_device");
			assert(false); return false;
		}

		// 创建交换链

		DXGI_SWAP_CHAIN_DESC1 desc = getDefaultSwapChainInfo7();
		desc.Width = mode.width;
		desc.Height = mode.height;

		DXGI_SWAP_CHAIN_FULLSCREEN_DESC descf = {};
		
		if (windowed)
		{
			// 仅限窗口模式
			if (flip)
			{
				// 配置 FLIP 交换链模型
				if (m_device->IsTearingSupport()) // Windows 10 且要求系统支持
				{
					desc.BufferCount = 2;
					desc.Scaling = DXGI_SCALING_NONE;
					desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
					desc.Flags |= DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
				}
				else if (m_device->IsFlipDiscardSupport()) // Windows 10
				{
					desc.BufferCount = 2;
					desc.Scaling = DXGI_SCALING_NONE;
					desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
				}
				else if (m_device->IsFlipSequentialSupport()) // Windows 8
				{
					desc.BufferCount = 2;
					desc.Scaling = DXGI_SCALING_NONE;
					desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
				}
				// 进一步配置低延迟呈现
				if (latency_event)
				{
					if (m_device->IsFrameLatencySupport()) // Windows 8.1
					{
						desc.BufferCount = 3; // 这时候需要三重缓冲
						desc.Flags |= DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;
					}
				}
			}
		}
		else
		{
			// 允许修改显示器显示模式
			desc.Flags |= DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
			// 配置全屏模式
			descf.RefreshRate.Numerator = mode.refresh_rate.numerator;
			descf.RefreshRate.Denominator = mode.refresh_rate.denominator;
			descf.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_PROGRESSIVE;
			descf.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
			descf.Windowed = TRUE; // 创建交换链后暂时保持在窗口化状态
		}

		m_swapchain_flags = desc.Flags;

		hr = gHR = m_device->GetDXGIFactory2()->CreateSwapChainForHwnd(
			m_device->GetD3D11Device(),
			m_window->GetWindow(),
			&desc,
			windowed ? NULL : &descf,
			NULL,
			&dxgi_swapchain);
		if (FAILED(hr))
		{
			i18n_core_system_call_report_error("IDXGIFactory2::CreateSwapChainForHwnd");
			assert(false); return false;
		}

		// 关闭傻逼快捷键，别他妈乱切换了

		// 注意这里他妈的有坑，新创建的 DXGI 工厂和交换链内部的的不是同一个
		hr = gHR = Platform::RuntimeLoader::DXGI::MakeSwapChainWindowAssociation(
			dxgi_swapchain.Get(), DXGI_MWA_NO_ALT_ENTER);
		if (FAILED(hr))
		{
			i18n_core_system_call_report_error("IDXGIFactory1::MakeWindowAssociation -> DXGI_MWA_NO_ALT_ENTER");
			assert(false); return false;
		}

		// 设置最大帧延迟为 1

		hr = gHR = Platform::RuntimeLoader::DXGI::SetDeviceMaximumFrameLatency(
			dxgi_swapchain.Get(), 1);
		if (FAILED(hr))
		{
			i18n_core_system_call_report_error("IDXGIDevice1::SetMaximumFrameLatency -> 1");
			assert(false); return false;
		}

		if (m_swapchain_flags & DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT)
		{
			HANDLE event_handle{};
			hr = gHR = Platform::RuntimeLoader::DXGI::SetSwapChainMaximumFrameLatency(
				dxgi_swapchain.Get(), 1, &event_handle);
			if (FAILED(hr))
			{
				i18n_core_system_call_report_error("IDXGISwapChain2::SetMaximumFrameLatency -> 1");
				assert(false); return false;
			}
			if (!event_handle)
			{
				i18n_core_system_call_report_error("IDXGISwapChain2::GetFrameLatencyWaitableObject");
				assert(false); return false;
			}
			dxgi_swapchain_event.Attach(event_handle);
		}

		//i18n_log_info("[core].SwapChain_D3D11.created_swapchain");

		auto refresh_rate_string = fmt::format("{:.2f}Hz", (double)mode.refresh_rate.numerator / (double)mode.refresh_rate.denominator);
		if (windowed) refresh_rate_string = i18n("DXGI.DisplayMode.RefreshRate.Desktop");
		std::string_view swapchain_model = i18n("DXGI.SwapChain.SwapEffect.Discard");
		if (desc.SwapEffect == DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL) swapchain_model = i18n("DXGI.SwapChain.SwapEffect.FlipSequential");
		if (desc.SwapEffect == DXGI_SWAP_EFFECT_FLIP_DISCARD) swapchain_model = i18n("DXGI.SwapChain.SwapEffect.FlipDiscard");
		auto enable_or_disable = [](bool v) -> std::string_view { return v ? i18n("Enable") : i18n("Disable"); };
		i18n_log_info_fmt("[core].SwapChain_D3D11.created_swapchain_info_fmt"
			, mode.width, mode.height, refresh_rate_string
			, enable_or_disable(!windowed)
			, swapchain_model
			, enable_or_disable(m_swapchain_flags & DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING)
			, enable_or_disable(m_swapchain_flags & DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT)
		);

		// 渲染附件

		if (!no_attachment)
		{
			if (!createRenderAttachment()) return false;
		}

		// 标记

		m_swapchain_want_present_reset = TRUE;

		return true;
	}
	bool SwapChain_D3D11::createExclusiveFullscreenSwapChain(DXGI_MODE_DESC1 const& mode, bool no_attachment)
	{
		_log("createSwapChain");

		i18n_log_info("[core].SwapChain_D3D11.start_creating_swapchain");

		// 必须成功的操作

		if (!m_window->GetWindow())
		{
			i18n_log_error("[core].SwapChain_D3D11.create_swapchain_failed_null_window");
			assert(false); return false;
		}
		if (!m_device->GetDXGIFactory2()) // 强制要求平台更新
		{
			i18n_log_error("[core].SwapChain_D3D11.create_swapchain_failed_null_DXGI");
			assert(false); return false;
		}
		if (!m_device->GetD3D11Device())
		{
			i18n_log_error("[core].SwapChain_D3D11.create_swapchain_failed_null_device");
			assert(false); return false;
		}

		HRNew;

		// 创建交换链

		DXGI_SWAP_CHAIN_DESC1 info = getDefaultSwapChainInfo7();
		info.Width = mode.Width;
		info.Height = mode.Height;
		info.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

		if (m_device->IsTearingSupport() && platform::WindowsVersion::Is10Build17763())
		{
			info.BufferCount = 2;
			info.Scaling = DXGI_SCALING_NONE;
			info.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		}

		DXGI_SWAP_CHAIN_FULLSCREEN_DESC fullscreen_info = {};
		fullscreen_info.RefreshRate = mode.RefreshRate;
		fullscreen_info.ScanlineOrdering = mode.ScanlineOrdering;
		fullscreen_info.Scaling = mode.Scaling;
		fullscreen_info.Windowed = TRUE; // 初始化为窗口模式，后续再进入全屏

		m_swapchain_flags = info.Flags;

		HRGet = m_device->GetDXGIFactory2()->CreateSwapChainForHwnd(
			m_device->GetD3D11Device(),
			m_window->GetWindow(),
			&info, &fullscreen_info, NULL,
			&dxgi_swapchain);
		HRCheckCallReturnBool("IDXGIFactory2::CreateSwapChainForHwnd");
		
		// 关闭傻逼快捷键，别他妈乱切换了

		// 注意这里他妈的有坑，新创建的 DXGI 工厂和交换链内部的的不是同一个
		HRGet = Platform::RuntimeLoader::DXGI::MakeSwapChainWindowAssociation(
			dxgi_swapchain.Get(), DXGI_MWA_NO_ALT_ENTER);
		HRCheckCallReturnBool("IDXGIFactory2::MakeWindowAssociation -> DXGI_MWA_NO_ALT_ENTER");
		
		// 设置最大帧延迟为 1

		HRGet = Platform::RuntimeLoader::DXGI::SetDeviceMaximumFrameLatency(
			dxgi_swapchain.Get(), 1);
		HRCheckCallReturnBool("IDXGIDevice1::SetMaximumFrameLatency -> 1");
		
		//i18n_log_info("[core].SwapChain_D3D11.created_swapchain");

		auto refresh_rate_string = fmt::format("{:.2f}Hz", (double)mode.RefreshRate.Numerator / (double)mode.RefreshRate.Denominator);
		std::string_view swapchain_model = i18n("DXGI.SwapChain.SwapEffect.Discard");
		auto enable_or_disable = [](bool v) -> std::string_view { return v ? i18n("Enable") : i18n("Disable"); };
		i18n_log_info_fmt("[core].SwapChain_D3D11.created_swapchain_info_fmt"
			, mode.Width, mode.Height, refresh_rate_string
			, enable_or_disable(true)
			, swapchain_model
			, enable_or_disable(false) // DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING
			, enable_or_disable(false) // DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT
		);

		// 渲染附件

		if (!no_attachment)
		{
			if (!createRenderAttachment()) return false;
		}

		// 标记

		m_swapchain_want_present_reset = TRUE;

		return true;
	}
	void SwapChain_D3D11::destroySwapChain()
	{
		_log("destroySwapChain");

		//waitFrameLatency(INFINITE, true);
		destroyDirectCompositionResources();
		destroyRenderAttachment();
		if (dxgi_swapchain)
		{
			// 退出独占全屏
			BOOL bFullscreen = FALSE;
			Microsoft::WRL::ComPtr<IDXGIOutput> dxgi_output;
			HRESULT hr = gHR = dxgi_swapchain->GetFullscreenState(&bFullscreen, &dxgi_output);
			if (bFullscreen)
			{
				i18n_log_info("[core].SwapChain_D3D11.leave_exclusive_fullscreen");
				hr = gHR = dxgi_swapchain->SetFullscreenState(FALSE, NULL);
				if (FAILED(hr))
				{
					i18n_core_system_call_report_error("IDXGISwapChain::SetFullscreenState -> FALSE");
				}
			}
		}
		dxgi_swapchain_event.Close();
		dxgi_swapchain.Reset();
	}
	bool SwapChain_D3D11::updateLetterBoxingRendererTransform()
	{
		_log("updateLetterBoxingRendererTransform");

		return m_scaling_renderer.UpdateTransform(
			m_canvas_d3d11_srv.Get(),
			m_swap_chain_d3d11_rtv.Get()
		);
	}
	void SwapChain_D3D11::applyRenderAttachment()
	{
		//_log("applyRenderAttachment");

		if (auto* ctx = m_device->GetD3D11DeviceContext())
		{
			ID3D11RenderTargetView* rtvs[1] = { m_canvas_d3d11_rtv.Get() };
			ctx->OMSetRenderTargets(1, rtvs, m_canvas_d3d11_dsv.Get());
		}
	}
	void SwapChain_D3D11::clearRenderAttachment()
	{
		//_log("clearRenderAttachment");

		if (auto* ctx = m_device->GetD3D11DeviceContext())
		{
			if (m_canvas_d3d11_rtv)
			{
				FLOAT const clear_color[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
				ctx->ClearRenderTargetView(m_canvas_d3d11_rtv.Get(), clear_color);
			}
			if (m_canvas_d3d11_dsv)
			{
				ctx->ClearDepthStencilView(m_canvas_d3d11_dsv.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0u);
			}
		}
	}
	void SwapChain_D3D11::waitFrameLatency(uint32_t timeout, bool reset)
	{
		//_log("waitFrameLatency");

		if (m_swapchain_flags & DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT)
		{
			if (reset && dxgi_swapchain)
			{
				HRESULT hr = gHR = dxgi_swapchain->Present(1, DXGI_PRESENT_RESTART);
				if (FAILED(hr))
				{
					i18n_core_system_call_report_error("IDXGISwapChain::Present -> (1, DXGI_PRESENT_RESTART)");
				}
			}
			if (dxgi_swapchain_event.IsValid())
			{
				DWORD const result = WaitForSingleObject(dxgi_swapchain_event.Get(), timeout);
				if (!(result == WAIT_OBJECT_0 || result == WAIT_TIMEOUT))
				{
					gHRLastError;
					i18n_core_system_call_report_error("WaitForSingleObject");
				}
			}
		}
	}
	bool SwapChain_D3D11::enterExclusiveFullscreenTemporarily()
	{
		if (m_swapchain_last_windowed)
		{
			return true;
		}
		if (!dxgi_swapchain)
		{
			assert(false); return false;
		}

		HRNew;

		BOOL get_state = FALSE;
		HRGet = dxgi_swapchain->GetFullscreenState(&get_state, NULL);
		HRCheckCallReturnBool("IDXGISwapChain::GetFullscreenState");

		if (get_state)
		{
			return true;
		}

		_log("IDXGISwapChain::SetFullscreenState -> TRUE\n");
		i18n_log_info("[core].SwapChain_D3D11.enter_exclusive_fullscreen");
		HRGet = dxgi_swapchain->SetFullscreenState(TRUE, NULL);
		HRCheckCallReturnBool("IDXGISwapChain::SetFullscreenState -> TRUE");

		return true;
	}
	bool SwapChain_D3D11::leaveExclusiveFullscreenTemporarily()
	{
		if (m_swapchain_last_windowed)
		{
			return true;
		}
		if (!dxgi_swapchain)
		{
			assert(false); return false;
		}

		HRNew;

		BOOL get_state = FALSE;
		HRGet = dxgi_swapchain->GetFullscreenState(&get_state, NULL);
		HRCheckCallReturnBool("IDXGISwapChain::GetFullscreenState");

		if (!get_state)
		{
			return true;
		}

		_log("IDXGISwapChain::SetFullscreenState -> FALSE\n");
		i18n_log_info("[core].SwapChain_D3D11.leave_exclusive_fullscreen");
		HRGet = dxgi_swapchain->SetFullscreenState(FALSE, NULL);
		HRCheckCallReturnBool("IDXGISwapChain::SetFullscreenState -> FALSE");

		m_window->setLayer(WindowLayer::Normal); // 强制取消窗口置顶

		return true;
	}
	bool SwapChain_D3D11::enterExclusiveFullscreen()
	{
		//return false;

		assert(dxgi_swapchain);
		if (!dxgi_swapchain) return false;

		assert(m_canvas_size.x > 0 && m_canvas_size.y > 0);
		if (m_canvas_size.x == 0 || m_canvas_size.y == 0) return false;

		DXGI_MODE_DESC1 display_mode = {};
		if (!findBestDisplayMode(dxgi_swapchain.Get(), m_canvas_size, display_mode)) return false;

		dispatchEvent(EventType::SwapChainDestroy);
		destroySwapChain();

		if (!m_window->getRedirectBitmapEnable())
		{
			m_window->setRedirectBitmapEnable(true);
			if (!m_window->recreateWindow()) return false;
			if (m_swapchain_flip_enabled)
			{
				m_swapchain_flip_enabled = FALSE;
			}
		}
		else if (m_swapchain_flip_enabled)
		{
			if (!m_window->recreateWindow()) return false;
			m_swapchain_flip_enabled = FALSE;
		}

		m_window->setSize({ display_mode.Width, display_mode.Height });

		DisplayMode mode = {
			.width = display_mode.Width,
			.height = display_mode.Height,
			.refresh_rate = Rational(display_mode.RefreshRate.Numerator, display_mode.RefreshRate.Denominator),
			.format = Format::B8G8R8A8_UNORM, // 未使用……
		};
		if (!createExclusiveFullscreenSwapChain(display_mode, true)) // 稍后创建渲染附件
		{
			return false;
		}

		HRNew;

		// 进入全屏
		i18n_log_info("[core].SwapChain_D3D11.enter_exclusive_fullscreen");
		HRGet = dxgi_swapchain->SetFullscreenState(TRUE, NULL);
		HRCheckCallReturnBool("IDXGISwapChain::SetFullscreenState -> TRUE");

		// 需要重设交换链大小（特别是 Flip 交换链模型）
		HRGet = dxgi_swapchain->ResizeBuffers(0, mode.width, mode.height, DXGI_FORMAT_UNKNOWN, m_swapchain_flags);
		HRCheckCallReturnBool("IDXGISwapChain::ResizeBuffers");
		
		// 创建渲染附件
		if (!createRenderAttachment())
		{
			return false;
		}
		if (!updateLetterBoxingRendererTransform()) return false;

		// 记录状态
		m_swapchain_last_mode = mode;
		m_swapchain_last_windowed = FALSE;
		m_swapchain_last_flip = FALSE;
		m_swapchain_last_latency_event = FALSE;
		m_init = TRUE;
		if (m_device->IsTearingSupport() && platform::WindowsVersion::Is10Build17763())
		{
			m_swapchain_last_flip = TRUE;
		}

		// 广播
		dispatchEvent(EventType::SwapChainCreate);
		
		return true;
	}
	bool SwapChain_D3D11::leaveExclusiveFullscreen()
	{
		HRNew;

		BOOL get_state = FALSE;
		HRGet = dxgi_swapchain->GetFullscreenState(&get_state, NULL);
		HRCheckCallReturnBool("IDXGISwapChain::GetFullscreenState");

		if (get_state)
		{
			i18n_log_info("[core].SwapChain_D3D11.leave_exclusive_fullscreen");
			HRGet = dxgi_swapchain->SetFullscreenState(FALSE, NULL);
			HRCheckCallReturnBool("IDXGISwapChain::SetFullscreenState -> FALSE");
		}
		
		m_swapchain_last_windowed = TRUE;

		return true;
	}

	bool SwapChain_D3D11::createDirectCompositionResources()
	{
		_log("createDirectCompositionResources");

		// 我们限制 DirectComposition 仅在 Windows 10+ 使用

		HRESULT hr = S_OK;

		// 必须成功的操作

		Microsoft::WRL::ComPtr<IDXGIDevice> dxgi_device;
		hr = gHR = m_device->GetD3D11Device()->QueryInterface(IID_PPV_ARGS(&dxgi_device));
		if (FAILED(hr))
		{
			i18n_core_system_call_report_error("ID3D11Device::QueryInterface -> IDXGIDevice");
			assert(false); return false;
		}

		// 创建基本组件

		hr = gHR = dcomp_loader.CreateDevice(dxgi_device.Get(), IID_PPV_ARGS(&dcomp_desktop_device));
		if (FAILED(hr))
		{
			i18n_core_system_call_report_error("DCompositionCreateDevice");
			assert(false); return false;
		}

	#ifdef _DEBUG
		Microsoft::WRL::ComPtr<IDCompositionDeviceDebug> dcomp_device_debug;
		HRGet = dcomp_desktop_device.As(&dcomp_device_debug);
		if (SUCCEEDED(hr))
		{
			HRGet = dcomp_device_debug->EnableDebugCounters();
		}
	#endif

		hr = gHR = dcomp_desktop_device->CreateTargetForHwnd(m_window->GetWindow(), TRUE, &dcomp_target);
		if (FAILED(hr))
		{
			i18n_core_system_call_report_error("IDCompositionDesktopDevice::CreateTargetForHwnd");
			assert(false); return false;
		}

		hr = gHR = dcomp_desktop_device->CreateVisual(&dcomp_visual_root);
		if (FAILED(hr))
		{
			i18n_core_system_call_report_error("IDCompositionDesktopDevice::CreateVisual");
			assert(false); return false;
		}

		hr = gHR = dcomp_desktop_device->CreateVisual(&dcomp_visual_background);
		if (FAILED(hr))
		{
			i18n_core_system_call_report_error("IDCompositionDesktopDevice::CreateVisual");
			assert(false); return false;
		}

		hr = gHR = dcomp_desktop_device->CreateVisual(&dcomp_visual_swap_chain);
		if (FAILED(hr))
		{
			i18n_core_system_call_report_error("IDCompositionDesktopDevice::CreateVisual");
			assert(false); return false;
		}

		// 初始化背景

		hr = gHR = dcomp_desktop_device->CreateSurface(
			BACKGROUND_W, BACKGROUND_H,
			COLOR_BUFFER_FORMAT,
			DXGI_ALPHA_MODE_IGNORE,
			&dcomp_surface_background);
		if (FAILED(hr))
		{
			i18n_core_system_call_report_error("IDCompositionDesktopDevice::CreateSurface");
			assert(false); return false;
		}
		else
		{
			Microsoft::WRL::ComPtr<IDXGISurface> dxgi_surface;
			POINT offset = { 0, 0 };
			hr = gHR = dcomp_surface_background->BeginDraw(NULL, IID_PPV_ARGS(&dxgi_surface), &offset);
			if (FAILED(hr))
			{
				i18n_core_system_call_report_error("IDCompositionSurface::BeginDraw");
				assert(false); return false;
			}

			Microsoft::WRL::ComPtr<ID3D11Resource> d3d11_res;
			hr = gHR = dxgi_surface.As(&d3d11_res);
			if (FAILED(hr))
			{
				i18n_core_system_call_report_error("IDXGISurface::QueryInterface -> ID3D11Resource");
				assert(false); return false;
			}

			Microsoft::WRL::ComPtr<ID3D11RenderTargetView> d3d11_dcomp_bg_rtv;
			hr = gHR = m_device->GetD3D11Device()->CreateRenderTargetView(d3d11_res.Get(), NULL, &d3d11_dcomp_bg_rtv);
			if (FAILED(hr))
			{
				i18n_core_system_call_report_error("ID3D11Device::CreateRenderTargetView");
				assert(false); return false;
			}

			FLOAT const clear_color[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
			m_device->GetD3D11DeviceContext()->ClearRenderTargetView(d3d11_dcomp_bg_rtv.Get(), clear_color);
			m_device->GetD3D11DeviceContext()->Flush();

			hr = gHR = dcomp_surface_background->EndDraw();
			if (FAILED(hr))
			{
				i18n_core_system_call_report_error("IDCompositionSurface::EndDraw");
				assert(false); return false;
			}
		}

		hr = gHR = dcomp_visual_background->SetContent(dcomp_surface_background.Get());
		if (FAILED(hr))
		{
			i18n_core_system_call_report_error("IDCompositionVisual2::SetContent");
			assert(false); return false;
		}

		// 把交换链塞进可视物

		hr = gHR = dcomp_visual_swap_chain->SetContent(dxgi_swapchain.Get());
		if (FAILED(hr))
		{
			i18n_core_system_call_report_error("IDCompositionVisual2::SetContent");
			assert(false); return false;
		}

		HRGet = dcomp_visual_swap_chain->SetBitmapInterpolationMode(DCOMPOSITION_BITMAP_INTERPOLATION_MODE_LINEAR);
		HRCheckCallReturnBool("IDCompositionVisual2::SetBitmapInterpolationMode -> DCOMPOSITION_BITMAP_INTERPOLATION_MODE_LINEAR");

		// 构建视觉树

		hr = gHR = dcomp_visual_root->AddVisual(dcomp_visual_background.Get(), TRUE, NULL);
		if (FAILED(hr))
		{
			i18n_core_system_call_report_error("IDCompositionVisual2::AddVisual");
			assert(false); return false;
		}

		hr = gHR = dcomp_visual_root->AddVisual(dcomp_visual_swap_chain.Get(), FALSE, NULL);
		if (FAILED(hr))
		{
			i18n_core_system_call_report_error("IDCompositionVisual2::AddVisual");
			assert(false); return false;
		}

		hr = gHR = dcomp_target->SetRoot(dcomp_visual_root.Get());
		if (FAILED(hr))
		{
			i18n_core_system_call_report_error("IDCompositionTarget::SetRoot");
			assert(false); return false;
		}

		// 设置变换并提交

		if (!updateDirectCompositionTransform()) return false;

		return true;
	}
	void SwapChain_D3D11::destroyDirectCompositionResources()
	{
		_log("destroyDirectCompositionResources");

		// 我也不知道为什么要这样清理，但是不这么做的话会出现内存不足，后续创建设备和资源的操作会失败

		HRNew;

		if (dcomp_target)
		{
			HRGet = dcomp_target->SetRoot(NULL);
		}
		if (dcomp_visual_root)
		{
			HRGet = dcomp_visual_root->RemoveAllVisuals();
		}
		if (dcomp_visual_background)
		{
			HRGet = dcomp_visual_background->SetContent(NULL);
		}
		if (dcomp_visual_swap_chain)
		{
			HRGet = dcomp_visual_swap_chain->SetContent(NULL);
		}

		dcomp_target.Reset();
		dcomp_visual_root.Reset();
		dcomp_visual_background.Reset();
		dcomp_visual_swap_chain.Reset();
		dcomp_surface_background.Reset();

		if (dcomp_desktop_device)
		{
			HRGet = dcomp_desktop_device->Commit();
			HRGet = dcomp_desktop_device->WaitForCommitCompletion();
		}

		dcomp_desktop_device.Reset();
	}
	bool SwapChain_D3D11::updateDirectCompositionTransform()
	{
		_log("updateDirectCompositionTransform");

		HRNew;

		// 必须成功的操作

		DXGI_SWAP_CHAIN_DESC1 desc1 = {};
		HRGet = dxgi_swapchain->GetDesc1(&desc1);
		HRCheckCallReturnBool("IDXGISwapChain1::GetDesc1");
		
		RECT rc = {};
		if (!GetClientRect(m_window->GetWindow(), &rc))
		{
			HRGet = HRESULT_FROM_WIN32(GetLastError());
			HRCheckCallReturnBool("GetClientRect");
		}
		auto const window_size_u = Vector2U(
			(uint32_t)(rc.right - rc.left),
			(uint32_t)(rc.bottom - rc.top));

		// 让背景铺满整个画面

		auto const background_mat = D2D1::Matrix3x2F::Scale(
			(float)window_size_u.x / (float)BACKGROUND_W,
			(float)window_size_u.y / (float)BACKGROUND_H
		);
		HRGet = dcomp_visual_background->SetTransform(background_mat);
		HRCheckCallReturnBool("IDCompositionVisual2::SetTransform");
		
		// 设置交换链内容内接放大

		DXGI_MATRIX_3X2_F mat{};
		if (makeLetterboxing(window_size_u, Vector2U(desc1.Width, desc1.Height), mat))
		{
			D2D_MATRIX_3X2_F const mat_d2d = {
				mat._11, mat._12,
				mat._21, mat._22,
				mat._31, mat._32,
			};
			HRGet = dcomp_visual_swap_chain->SetTransform(mat_d2d);
			HRCheckCallReturnBool("IDCompositionVisual2::SetTransform");
		}
		else
		{
			auto const mat_d2d = D2D1::Matrix3x2F::Identity();
			HRGet = dcomp_visual_swap_chain->SetTransform(mat_d2d);
			HRCheckCallReturnBool("IDCompositionVisual2::SetTransform");
		}

		// 提交

		if (!commitDirectComposition()) return false;

		return true;
	}
	bool SwapChain_D3D11::commitDirectComposition()
	{
		_log("commitDirectComposition");

		HRNew;

		HRGet = dcomp_desktop_device->Commit();
		HRCheckCallReturnBool("IDCompositionDesktopDevice::Commit");
		
		HRGet = dcomp_desktop_device->WaitForCommitCompletion();
		HRCheckCallReturnBool("IDCompositionDesktopDevice::WaitForCommitCompletion");
		
		return true;
	}
	bool SwapChain_D3D11::createCompositionSwapChain(Vector2U size, bool latency_event)
	{
		_log("createCompositionSwapChain");

		// 我们限制 DirectComposition 仅在 Windows 10+ 使用

		HRNew;

		i18n_log_info("[core].SwapChain_D3D11.start_creating_swapchain");

		// 检查组件

		if (!m_window->GetWindow())
		{
			i18n_log_error("[core].SwapChain_D3D11.create_swapchain_failed_null_window");
			assert(false); return false;
		}
		if (!m_device->GetDXGIFactory2()) // 要求平台更新
		{
			i18n_log_error("[core].SwapChain_D3D11.create_swapchain_failed_null_DXGI");
			assert(false); return false;
		}
		if (!m_device->GetD3D11Device())
		{
			i18n_log_error("[core].SwapChain_D3D11.create_swapchain_failed_null_device");
			assert(false); return false;
		}

		// 填充交换链描述

		DXGI_SWAP_CHAIN_DESC1 desc = getDefaultSwapChainInfo10();
		desc.Width = size.x;
		desc.Height = size.y;
		desc.Scaling = DXGI_SCALING_STRETCH; // DirectComposition 要求这个缩放模式

		if (m_device->IsTearingSupport()) // Windows 10+ 且要求系统支持
		{
			desc.Flags |= DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING; // 允许撕裂
		}
		if (latency_event)
		{
			desc.BufferCount = 3;
			desc.Flags |= DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT; // 低延迟渲染
		}

		m_swapchain_flags = desc.Flags;

		// 创建交换链

		HRGet = m_device->GetDXGIFactory2()->CreateSwapChainForComposition(
			m_device->GetD3D11Device(),
			&desc, NULL,
			&dxgi_swapchain);
		HRCheckCallReturnBool("IDXGIFactory2::CreateSwapChainForComposition");
		
		// 设置最大帧延迟为 1

		HRGet = Platform::RuntimeLoader::DXGI::SetDeviceMaximumFrameLatency(dxgi_swapchain.Get(), 1);
		HRCheckCallReturnBool("IDXGIDevice1::SetMaximumFrameLatency -> 1");
		
		if (m_swapchain_flags & DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT)
		{
			HANDLE event_handle{};
			HRGet = Platform::RuntimeLoader::DXGI::SetSwapChainMaximumFrameLatency(
				dxgi_swapchain.Get(), 1, &event_handle);
			HRCheckCallReturnBool("IDXGISwapChain2::SetMaximumFrameLatency -> 1");
			dxgi_swapchain_event.Attach(event_handle);
		}

		// 打印信息

		auto refresh_rate_string = i18n("DXGI.DisplayMode.RefreshRate.Desktop");
		std::string_view swapchain_model = i18n("DXGI.SwapChain.SwapEffect.FlipSequential");
		if (desc.SwapEffect == DXGI_SWAP_EFFECT_FLIP_DISCARD) swapchain_model = i18n("DXGI.SwapChain.SwapEffect.FlipDiscard");
		auto enable_or_disable = [](bool v) -> std::string_view { return v ? i18n("Enable") : i18n("Disable"); };
		i18n_log_info_fmt("[core].SwapChain_D3D11.created_swapchain_info_fmt"
			, size.x, size.y, refresh_rate_string
			, i18n("Disable") // 没有独占全屏
			, swapchain_model
			, enable_or_disable(m_swapchain_flags & DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING)
			, enable_or_disable(m_swapchain_flags & DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT)
		);

		// 渲染附件

		if (!createRenderAttachment()) return false;

		// 创建合成器

		if (!createDirectCompositionResources()) return false;

		// 标记

		m_swapchain_want_present_reset = TRUE;

		return true;
	}

	bool SwapChain_D3D11::createSwapChainRenderTarget()
	{
		_log("createSwapChainRenderTarget");

		if (!dxgi_swapchain)
		{
			assert(false); return false;
		}

		if (!m_device->GetD3D11Device())
		{
			assert(false); return false;
		}

		HRNew;

		Microsoft::WRL::ComPtr<ID3D11Texture2D> dxgi_surface;
		HRGet = dxgi_swapchain->GetBuffer(0, IID_PPV_ARGS(&dxgi_surface));
		HRCheckCallReturnBool("IDXGISwapChain::GetBuffer -> 0");
		
		// TODO: 线性颜色空间
		HRGet = m_device->GetD3D11Device()->CreateRenderTargetView(dxgi_surface.Get(), NULL, &m_swap_chain_d3d11_rtv);
		HRCheckCallReturnBool("ID3D11Device::CreateRenderTargetView");

		return true;
	}
	void SwapChain_D3D11::destroySwapChainRenderTarget()
	{
		_log("destroySwapChainRenderTarget");

		if (m_device->GetD3D11DeviceContext())
		{
			m_device->GetD3D11DeviceContext()->ClearState();
			m_device->GetD3D11DeviceContext()->Flush();
		}
		m_swap_chain_d3d11_rtv.Reset();
	}
	bool SwapChain_D3D11::createCanvasColorBuffer()
	{
		_log("createCanvasColorBuffer");

		if (m_canvas_size.x == 0 || m_canvas_size.y == 0)
		{
			assert(false); return false;
		}

		if (!m_device->GetD3D11Device())
		{
			assert(false); return false;
		}

		HRNew;

		// Color Buffer

		D3D11_TEXTURE2D_DESC cb_info = {};
		cb_info.Width = m_canvas_size.x;
		cb_info.Height = m_canvas_size.y;
		cb_info.MipLevels = 1;
		cb_info.ArraySize = 1;
		cb_info.Format = COLOR_BUFFER_FORMAT;
		cb_info.SampleDesc.Count = 1;
		cb_info.Usage = D3D11_USAGE_DEFAULT;
		cb_info.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

		Microsoft::WRL::ComPtr<ID3D11Texture2D> cb_texture;
		HRGet = m_device->GetD3D11Device()->CreateTexture2D(&cb_info, NULL, &cb_texture);
		HRCheckCallReturnBool("ID3D11Device::CreateTexture2D");

		// Shader Resource

		D3D11_SHADER_RESOURCE_VIEW_DESC srv_info = {};
		srv_info.Format = cb_info.Format; // TODO: 线性颜色空间
		srv_info.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		srv_info.Texture2D.MipLevels = cb_info.MipLevels;

		HRGet = m_device->GetD3D11Device()->CreateShaderResourceView(cb_texture.Get(), &srv_info, &m_canvas_d3d11_srv);
		HRCheckCallReturnBool("ID3D11Device::CreateShaderResourceView");

		// Render Target

		D3D11_RENDER_TARGET_VIEW_DESC rtv_info = {};
		rtv_info.Format = cb_info.Format; // TODO: 线性颜色空间
		rtv_info.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;

		HRGet = m_device->GetD3D11Device()->CreateRenderTargetView(cb_texture.Get(), &rtv_info, &m_canvas_d3d11_rtv);
		HRCheckCallReturnBool("ID3D11Device::CreateRenderTargetView");

		return true;
	}
	void SwapChain_D3D11::destroyCanvasColorBuffer()
	{
		_log("destroyCanvasColorBuffer");

		m_canvas_d3d11_srv.Reset();
		m_canvas_d3d11_rtv.Reset();
	}
	bool SwapChain_D3D11::createCanvasDepthStencilBuffer()
	{
		_log("createCanvasDepthStencilBuffer");

		if (m_canvas_size.x == 0 || m_canvas_size.y == 0)
		{
			assert(false); return false;
		}

		if (!m_device->GetD3D11Device())
		{
			assert(false); return false;
		}

		HRNew;

		// Depth Stencil Buffer

		D3D11_TEXTURE2D_DESC ds_info = {};
		ds_info.Width = m_canvas_size.x;
		ds_info.Height = m_canvas_size.y;
		ds_info.MipLevels = 1;
		ds_info.ArraySize = 1;
		ds_info.Format = DEPTH_BUFFER_FORMAT;
		ds_info.SampleDesc.Count = 1;
		ds_info.Usage = D3D11_USAGE_DEFAULT;
		ds_info.BindFlags = D3D11_BIND_DEPTH_STENCIL;

		Microsoft::WRL::ComPtr<ID3D11Texture2D> ds_texture;
		HRGet = m_device->GetD3D11Device()->CreateTexture2D(&ds_info, NULL, &ds_texture);
		HRCheckCallReturnBool("ID3D11Device::CreateTexture2D");

		// Depth Stencil View

		D3D11_DEPTH_STENCIL_VIEW_DESC dsv_info = {};
		dsv_info.Format = ds_info.Format;
		dsv_info.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;

		HRGet = m_device->GetD3D11Device()->CreateDepthStencilView(ds_texture.Get(), &dsv_info, &m_canvas_d3d11_dsv);
		HRCheckCallReturnBool("ID3D11Device::CreateDepthStencilView");

		return true;
	}
	void SwapChain_D3D11::destroyCanvasDepthStencilBuffer()
	{
		_log("destroyCanvasDepthStencilBuffer");

		m_canvas_d3d11_dsv.Reset();
	}
	bool SwapChain_D3D11::createRenderAttachment()
	{
		_log("createRenderAttachment");

		if (!createSwapChainRenderTarget()) return false;
		if (m_is_composition_mode)
		{
			// 此时画布颜色缓冲区就是交换链的后台缓冲区
			m_canvas_d3d11_srv.Reset(); // 不使用
			m_canvas_d3d11_rtv = m_swap_chain_d3d11_rtv;
		}
		else
		{
			if (!createCanvasColorBuffer()) return false;
		}
		if (!createCanvasDepthStencilBuffer()) return false;
		return true;
	}
	void SwapChain_D3D11::destroyRenderAttachment()
	{
		_log("destroyRenderAttachment");

		destroyCanvasDepthStencilBuffer();
		destroyCanvasColorBuffer();
		destroySwapChainRenderTarget();
	}

	bool SwapChain_D3D11::handleDirectCompositionWindowSize(Vector2U size)
	{
		_log("handleDirectCompositionWindowSize");

		if (size.x == 0 || size.y == 0)
		{
			assert(false); return false;
		}

		if (!dxgi_swapchain)
		{
			assert(false); return false;
		}
		
		// 此时交换链和画布一致，不应该修改交换链本身，而是修改合成变换

		if (!updateDirectCompositionTransform()) return false;

		return true;
	}
	bool SwapChain_D3D11::handleSwapChainWindowSize(Vector2U size)
	{
		_log("handleSwapChainWindowSize");

		if (size.x == 0 || size.y == 0)
		{
			assert(false); return false;
		}

		if (!dxgi_swapchain)
		{
			assert(false); return false;
		}

		// 此时交换链和画布分离，应该重新调整交换链尺寸

		HRNew;

		destroySwapChainRenderTarget();

		HRGet = dxgi_swapchain->ResizeBuffers(0, size.x, size.y, DXGI_FORMAT_UNKNOWN, m_swapchain_flags);
		HRCheckCallReturnBool("IDXGISwapChain::ResizeBuffers");

		if (!createSwapChainRenderTarget()) return false;
		m_swapchain_last_mode.width = size.x;
		m_swapchain_last_mode.height = size.y;

		if (!updateLetterBoxingRendererTransform()) return false;

		return true;
	}

	bool SwapChain_D3D11::setWindowMode(Vector2U size)
	{
		_log("setWindowMode");

		if (platform::WindowsVersion::Is10Build17763()
			&& !Platform::DesktopWindowManager::IsOverlayTestModeExists()
			&& m_device->IsTearingSupport()
			&& checkHardwareCompositionSupport(m_device->GetD3D11Device()))
		{
			// 开启条件：
			// 1. 交换链快速交换模式（DXGI_SWAP_EFFECT_FLIP_DISCARD）从 Windows 10 开始支持
			// 2. 允许画面撕裂（立即刷新）从 Windows 10 开始支持，但是也需要硬件、驱动、系统更新等才能支持
			// 3. 在 Windows 10 1709 (16299) Fall Creators Update 中
			//    修复了 Frame Latency Waitable Object 和 SetMaximumFrameLatency 实际上至少有 2 帧的问题
			// 4. DirectComposition 从 Windows 8 开始支持
			// 5. 由于 MPO 存在一些已知的问题，导致很多用户的会选择关闭自己设备的 MPO 功能，
			//    在关闭 MPO 后，画面呈现方式会变成 Composed: Flip 而不是 Hardware Composed: Independent Flip
			//    这会导致延迟增加 10 倍以上，因此我们需要回退到正常的铺满窗口的交换链
			m_is_composition_mode = true;
			return setCompositionWindowMode(size);
		}
		else
		{
			m_is_composition_mode = false;
		}

		bool flip_model = false;
		bool latency_event = false;
		if (m_device->IsTearingSupport() && platform::WindowsVersion::Is10Build17763())
		{
			flip_model = true;
			latency_event = true;
		}

		if (size.x < 1 || size.y < 1)
		{
			i18n_log_error_fmt("[core].SwapChain_D3D11.create_swapchain_failed_invalid_size_fmt", size.x, size.y);
			return false;
		}
		dispatchEvent(EventType::SwapChainDestroy);
		destroySwapChain();

		if (!m_window->getRedirectBitmapEnable())
		{
			m_window->setRedirectBitmapEnable(true);
			if (!m_window->recreateWindow()) return false;
			if (!flip_model && m_swapchain_flip_enabled)
			{
				m_swapchain_flip_enabled = FALSE;
			}
		}
		else if (!flip_model && m_swapchain_flip_enabled)
		{
			if (!m_window->recreateWindow()) return false;
			m_swapchain_flip_enabled = FALSE;
		}

		DisplayMode mode = {
			.width = size.x,
			.height = size.y,
			.refresh_rate = Rational(),
			.format = Format::B8G8R8A8_UNORM,
		};
		m_canvas_size = Vector2U(mode.width, mode.height);
		if (!createSwapChain(true, flip_model, latency_event, mode, false)) // 让它创建渲染附件
		{
			return false;
		}
		m_swapchain_last_mode = mode;
		m_swapchain_last_windowed = TRUE;
		m_swapchain_last_flip = flip_model;
		m_swapchain_last_latency_event = latency_event;
		m_init = TRUE;
		if (flip_model) m_swapchain_flip_enabled = TRUE;
		dispatchEvent(EventType::SwapChainCreate);

		if (!updateLetterBoxingRendererTransform()) return false;

		return true;
	}
	bool SwapChain_D3D11::setCompositionWindowMode(Vector2U size)
	{
		_log("setCompositionWindowMode");

		// 检查参数

		if (size.x < 1 || size.y < 1)
		{
			i18n_log_error_fmt("[core].SwapChain_D3D11.create_swapchain_failed_invalid_size_fmt", size.x, size.y);
			return false;
		}

		// 销毁旧交换链

		dispatchEvent(EventType::SwapChainDestroy);
		destroySwapChain();

		// 如果有重定向表面，则去除

		if (m_window->getRedirectBitmapEnable())
		{
			m_window->setRedirectBitmapEnable(false);
			if (!m_window->recreateWindow())
				return false;
		}

		// 创建交换链

		bool latency_event = true;
		m_canvas_size = size;
		if (!createCompositionSwapChain(size, latency_event))
		{
			return false;
		}

		// 更新数据

		m_swapchain_last_mode = DisplayMode{
			.width = size.x,
			.height = size.y,
			.refresh_rate = Rational(),
			.format = Format::B8G8R8A8_UNORM,
		};
		m_swapchain_last_windowed = TRUE;
		m_swapchain_last_flip = TRUE;
		m_swapchain_last_latency_event = latency_event;
		m_init = TRUE;
		m_swapchain_flip_enabled = TRUE; // 固定是开启过的

		// 通知各个组件交换链已重新创建

		dispatchEvent(EventType::SwapChainCreate);

		return true;
	}

	bool SwapChain_D3D11::setCanvasSize(Vector2U size)
	{
		_log("setCanvasSize");

		m_canvas_size = size;

		if (!dxgi_swapchain)
		{
			return true; // 当交换链还未初始化时，仅保存画布尺寸
		}

		dispatchEvent(EventType::SwapChainDestroy);

		if (m_is_composition_mode)
		{
			// 对于合成交换链，由于交换链和画布是一致的，所以要调整交换链尺寸

			destroyRenderAttachment();

			HRNew;
			HRGet = dxgi_swapchain->ResizeBuffers(0, size.x, size.y, DXGI_FORMAT_UNKNOWN, m_swapchain_flags);
			HRCheckCallReturnBool("IDXGISwapChain::ResizeBuffers");

			if (!createRenderAttachment()) return false;

			if (!updateDirectCompositionTransform()) return false;
		}
		else
		{
			// 对于普通交换链，由于画布是独立的，只需重新创建画布

			destroyCanvasDepthStencilBuffer();
			destroyCanvasColorBuffer();

			if (!createCanvasColorBuffer()) return false;
			if (!createCanvasDepthStencilBuffer()) return false;

			if (!updateLetterBoxingRendererTransform()) return false;
		}

		dispatchEvent(EventType::SwapChainCreate);

		return true;
	}

	void SwapChain_D3D11::waitFrameLatency()
	{
		if (m_swapchain_want_present_reset)
		{
			m_swapchain_want_present_reset = FALSE;
			waitFrameLatency(1000, true);
		}
		else
		{
			waitFrameLatency(1000, false);
		}
	}
	void SwapChain_D3D11::setVSync(bool enable)
	{
		m_swapchain_vsync = enable;
	}
	bool SwapChain_D3D11::present()
	{
		HRESULT hr = S_OK;

		// 缩放显示

		if (m_is_composition_mode)
		{
		}
		else
		{
			if (!m_scaling_renderer.Draw(
				m_canvas_d3d11_srv.Get(),
				m_swap_chain_d3d11_rtv.Get(),
				true
			)) return false;
		}

		// 呈现

		UINT const interval = m_swapchain_vsync ? 1 : 0;
		UINT flags = 0;
		if (!m_swapchain_vsync && (m_swapchain_flags & DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING))
		{
			flags |= DXGI_PRESENT_ALLOW_TEARING;
		}
		hr = gHR = dxgi_swapchain->Present(interval, flags);

		// 清空渲染状态并丢弃内容

		m_device->GetD3D11DeviceContext()->ClearState();
		m_device->GetD3D11DeviceContext1()->DiscardView(m_swap_chain_d3d11_rtv.Get());
		
		// 检查结果

		if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET)
		{
			// 设备丢失
			return m_device->handleDeviceLost();
		}
		else if (FAILED(hr))
		{
			i18n_core_system_call_report_error("IDXGISwapChain::Present");
			return false;
		}

		// 检查 DXGI 工厂有效性

		if (!m_device->validateDXGIFactory())
			return false;

		return true;
	}

	bool SwapChain_D3D11::saveSnapshotToFile(StringView path)
	{
		std::wstring wpath(utility::encoding::to_wide(path));

		HRESULT hr = S_OK;

		Microsoft::WRL::ComPtr<ID3D11Resource> d3d11_resource;
		m_canvas_d3d11_rtv->GetResource(&d3d11_resource);

		hr = gHR = DirectX::SaveWICTextureToFile(
			m_device->GetD3D11DeviceContext(),
			d3d11_resource.Get(),
			GUID_ContainerFormatJpeg,
			wpath.c_str(),
			&GUID_WICPixelFormat24bppBGR);
		if (FAILED(hr))
		{
			i18n_core_system_call_report_error("DirectX::SaveWICTextureToFile");
			return false;
		}

		return true;
	}

	SwapChain_D3D11::SwapChain_D3D11(Window_Win32* p_window, Device_D3D11* p_device)
		: m_window(p_window)
		, m_device(p_device)
	{
		m_scaling_renderer.AttachDevice(m_device->GetD3D11Device());
		m_window->addEventListener(this);
		m_device->addEventListener(this);
	}
	SwapChain_D3D11::~SwapChain_D3D11()
	{
		m_window->removeEventListener(this);
		m_device->removeEventListener(this);
		destroySwapChain();
		m_scaling_renderer.DetachDevice();
		assert(m_eventobj.size() == 0);
		assert(m_eventobj_late.size() == 0);
	}

	bool SwapChain_D3D11::create(Window_Win32* p_window, Device_D3D11* p_device, SwapChain_D3D11** pp_swapchain)
	{
		try
		{
			*pp_swapchain = new SwapChain_D3D11(p_window, p_device);
			return true;
		}
		catch (...)
		{
			*pp_swapchain = nullptr;
			return false;
		}
	}

	bool ISwapChain::create(IWindow* p_window, IDevice* p_device, ISwapChain** pp_swapchain)
	{
		try
		{
			*pp_swapchain = new SwapChain_D3D11(dynamic_cast<Window_Win32*>(p_window), dynamic_cast<Device_D3D11*>(p_device));
			return true;
		}
		catch (...)
		{
			*pp_swapchain = nullptr;
			return false;
		}
	}
}
