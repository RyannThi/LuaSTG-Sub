﻿#include <string>
#include <fstream>
#include "resource.h"
#include "AppFrame.h"
#include "Utility.h"
#include "ImGuiExtension.h"
#include "LuaWrapper/LuaAppFrame.hpp"
#include "Graphic/Test.h"

using namespace LuaSTGPlus;

////////////////////////////////////////////////////////////////////////////////
/// AppFrame
////////////////////////////////////////////////////////////////////////////////

LNOINLINE AppFrame& AppFrame::GetInstance()
{
	static AppFrame s_Instance;
	return s_Instance;
}

AppFrame::AppFrame() LNOEXCEPT
{
}

AppFrame::~AppFrame() LNOEXCEPT
{
	if (m_iStatus != AppStatus::NotInitialized && m_iStatus != AppStatus::Destroyed)
	{
		// 若没有销毁框架，则执行销毁
		Shutdown();
	}
}

#pragma region 脚本接口

void AppFrame::SetWindowed(bool v)LNOEXCEPT
{
	if (m_iStatus == AppStatus::Initializing)
		m_OptionWindowed = v;
	else if (m_iStatus == AppStatus::Running)
		spdlog::warn(u8"[luastg] SetWindowed: 试图在运行时更改窗口化模式");
}

void AppFrame::SetFPS(fuInt v)LNOEXCEPT
{
	m_OptionFPSLimit = (v > 1u) ? v : 1u; // 最低也得有1FPS每秒
}

void AppFrame::SetVsync(bool v)LNOEXCEPT
{
	if (m_iStatus == AppStatus::Initializing)
		m_OptionVsync = v;
	else if (m_iStatus == AppStatus::Running)
		spdlog::warn(u8"[luastg] SetVsync: 试图在运行时更改垂直同步模式");
}

void AppFrame::SetResolution(fuInt width, fuInt height)LNOEXCEPT
{
	if (m_iStatus == AppStatus::Initializing)
		m_OptionResolution.Set((float)width, (float)height);
	else if (m_iStatus == AppStatus::Running)
		spdlog::warn(u8"[luastg] SetResolution: 试图在运行时更改分辨率");
}

void AppFrame::SetTitle(const char* v)LNOEXCEPT
{
	try
	{
		m_OptionTitle = std::move(fcyStringHelper::MultiByteToWideChar(v, CP_UTF8));
		if (m_pMainWindow)
			m_pMainWindow->SetCaption(m_OptionTitle.c_str());
	}
	catch (const std::bad_alloc&)
	{
		spdlog::error(u8"[luastg] SetTitle: 内存不足");
	}
}

void AppFrame::SetSplash(bool v)LNOEXCEPT
{
	m_OptionCursor = v;
	if (m_pMainWindow)
		m_pMainWindow->HideMouse(!m_OptionCursor);
}

LNOINLINE bool AppFrame::ChangeVideoMode(int width, int height, bool windowed, bool vsync)LNOEXCEPT
{
	if (m_iStatus == AppStatus::Initialized)
	{
		// 切换到新的视频选项
		if (FCYOK(m_pRenderDev->SetBufferSize(
			(fuInt)width,
			(fuInt)height,
			windowed,
			vsync,
			false,
			F2DAALEVEL_NONE)))
		{
			spdlog::info(u8"[luastg] 视频模式切换成功 ({}x{} Vsync:{} Windowed:{}) -> ({}x{} Vsync:{} Windowed:{})",
				(int)m_OptionResolution.x, (int)m_OptionResolution.y, m_OptionVsync, m_OptionWindowed,
				width, height, vsync, windowed);

			m_OptionResolution.Set((float)width, (float)height);
			m_OptionWindowed = windowed;
			m_OptionVsync = vsync;

			// 切换窗口大小
			m_pMainWindow->SetBorderType(m_OptionWindowed ? F2DWINBORDERTYPE_FIXED : F2DWINBORDERTYPE_NONE);
			m_pMainWindow->SetClientRect(
				fcyRect(10.f, 10.f, 10.f + m_OptionResolution.x, 10.f + m_OptionResolution.y)
				);
			m_pMainWindow->SetTopMost(!m_OptionWindowed);
			m_pMainWindow->MoveToCenter();
			return true;
		}
		else
		{
			// 改变交换链大小失败后将窗口模式设为true
			m_OptionWindowed = true;

			// 切换窗口大小
			m_pMainWindow->SetBorderType(m_OptionWindowed ? F2DWINBORDERTYPE_FIXED : F2DWINBORDERTYPE_NONE);
			m_pMainWindow->SetClientRect(
				fcyRect(10.f, 10.f, 10.f + m_OptionResolution.x, 10.f + m_OptionResolution.y)
				);
			m_pMainWindow->SetTopMost(!m_OptionWindowed);
			m_pMainWindow->MoveToCenter();

			spdlog::error(u8"[luastg] 视频模式切换失败 ({}x{} Vsync:{} Windowed:{}) -> ({}x{} Vsync:{} Windowed:{})",
				(int)m_OptionResolution.x, (int)m_OptionResolution.y, m_OptionVsync, m_OptionWindowed,
				width, height, vsync, windowed);
		}
	}
	return false;
}

LNOINLINE bool AppFrame::UpdateVideoMode()LNOEXCEPT
{
	if (m_iStatus == AppStatus::Initialized)
	{
		// 切换到新的视频选项
		if (FCYOK(m_pRenderDev->SetBufferSize(
			(fuInt)m_OptionResolution.x,
			(fuInt)m_OptionResolution.y,
			m_OptionWindowed,
			m_OptionVsync,
			false,
			F2DAALEVEL_NONE)))
		{
			spdlog::info(u8"[luastg] 视频模式切换成功 ({}x{} Vsync:{} Windowed:{})",
				(int)m_OptionResolution.x, (int)m_OptionResolution.y, m_OptionVsync, m_OptionWindowed);
			// 切换窗口大小
			m_pMainWindow->SetBorderType(m_OptionWindowed ? F2DWINBORDERTYPE_FIXED : F2DWINBORDERTYPE_NONE);
			m_pMainWindow->SetClientRect(
				fcyRect(10.f, 10.f, 10.f + m_OptionResolution.x, 10.f + m_OptionResolution.y)
				);
			m_pMainWindow->SetTopMost(!m_OptionWindowed);
			m_pMainWindow->MoveToCenter();
			return true;
		}
		else
		{
			// 改变交换链大小失败后将窗口模式设为true
			m_OptionWindowed = true;
			
			// 切换窗口大小
			m_pMainWindow->SetBorderType(m_OptionWindowed ? F2DWINBORDERTYPE_FIXED : F2DWINBORDERTYPE_NONE);
			m_pMainWindow->SetClientRect(
				fcyRect(10.f, 10.f, 10.f + m_OptionResolution.x, 10.f + m_OptionResolution.y)
				);
			m_pMainWindow->SetTopMost(!m_OptionWindowed);
			m_pMainWindow->MoveToCenter();

			spdlog::error(u8"[luastg] 视频模式切换失败 ({}x{} Vsync:{} Windowed:{})",
				(int)m_OptionResolution.x, (int)m_OptionResolution.y, m_OptionVsync, m_OptionWindowed);
		}
	}
	return false;
}

LNOINLINE int AppFrame::LoadTextFile(lua_State* L, const char* path, const char *packname)LNOEXCEPT
{
	if (ResourceMgr::GetResourceLoadingLog()) {
		spdlog::info(u8"[luastg] 读取文本文件'{}'", path);
	}
	fcyRefPointer<fcyMemStream> tMemStream;
	if (!m_ResourceMgr.LoadFile(path, tMemStream, packname)) {
		spdlog::error(u8"[luastg] 无法加载文件'{}'", path);
		return 0;
	}
	lua_pushlstring(L, (char*)tMemStream->GetInternalBuffer(), (size_t)tMemStream->GetLength());
	tMemStream = nullptr;
	return 1;
}

#pragma endregion

#pragma region 框架函数

bool AppFrame::Init()LNOEXCEPT
{
	assert(m_iStatus == AppStatus::NotInitialized);
	
	spdlog::info(LUASTG_INFO);
	spdlog::info(u8"[luastg] 初始化引擎");
	m_iStatus = AppStatus::Initializing;
	
	//////////////////////////////////////// Lua初始化部分
	
	spdlog::info(u8"[luastg] 初始化luajit引擎");
	
	// 开启Lua引擎
	if (!OnOpenLuaEngine())
	{
		spdlog::info(u8"[luastg] 初始化luajit引擎失败");
		return false;
	}
	
	// 加载初始化脚本（可选）
	if (!OnLoadLaunchScriptAndFiles())
	{
		return false;
	}
	
	//////////////////////////////////////// 初始化引擎
	{
		// 为对象池分配空间
		spdlog::info(u8"[luastg] 初始化对象池，容量{}", LOBJPOOL_SIZE);
		try
		{
			m_GameObjectPool = std::make_unique<GameObjectPool>(L);
		}
		catch (const std::bad_alloc&)
		{
			spdlog::error(u8"[luastg] 无法为对象池分配内存");
			return false;
		}
		
		// 初始化fancy2d引擎
		spdlog::info(u8"[fancy2d] 初始化，窗口分辨率：{}x{}，垂直同步：{}，窗口化：{}",
			(int)m_OptionResolution.x, (int)m_OptionResolution.y, m_OptionVsync, m_OptionWindowed);
		
		struct : public f2dInitialErrListener
		{
			void OnErr(fuInt TimeTick, fcStr Src, fcStr Desc)
			{
				spdlog::error(u8"[fancy2d] [{}] {}", Src, Desc);
			}
		} tErrListener;
		
		if (FCYFAILED(CreateF2DEngineAndInit(
			F2DVERSION,
			fcyRect(0.f, 0.f, m_OptionResolution.x, m_OptionResolution.y),
			m_OptionTitle.c_str(),
			m_OptionWindowed,
			m_OptionVsync,
			F2DAALEVEL_NONE,
			this,
			&m_pEngine,
			&tErrListener
			)))
		{
			spdlog::error(u8"[fancy2d] 初始化失败");
			return false;
		}
		
		// 获取组件
		m_pMainWindow = m_pEngine->GetMainWindow();
		m_pRenderer = m_pEngine->GetRenderer();
		m_pRenderDev = m_pRenderer->GetDevice();
		m_pSoundSys = m_pEngine->GetSoundSys();
		m_pInputSys = m_pEngine->GetInputSys();
		
		// 打印设备信息
		f2dCPUInfo stCPUInfo = { 0 };
		m_pEngine->GetCPUInfo(stCPUInfo);
		spdlog::info(u8"[fancy2d] CPU {} {}", stCPUInfo.CPUBrandString, stCPUInfo.CPUString);
		spdlog::info(u8"[fancy2d] GPU {}", m_pRenderDev->GetDeviceName());
		
		// 设置窗口图标
		HICON hIcon = LoadIconW(GetModuleHandleW(NULL), MAKEINTRESOURCEW(IDI_APPICON));
		SendMessageW((HWND)m_pMainWindow->GetHandle(), WM_SETICON, (WPARAM)ICON_BIG  , (LPARAM)hIcon);
		SendMessageW((HWND)m_pMainWindow->GetHandle(), WM_SETICON, (WPARAM)ICON_SMALL, (LPARAM)hIcon);
		DestroyIcon(hIcon);
		
		// 默认关闭深度缓冲
		m_pRenderDev->SetZBufferEnable(false);
		m_pRenderDev->ClearZBuffer();
		
		// 创建渲染器
		spdlog::info(u8"[fancy2d] 创建2D渲染器，顶点容量{}，索引容量{}", 16384, 24576);
		if (FCYFAILED(m_pRenderDev->CreateGraphics2D(16384, 24576, &m_Graph2D)))
		{
			spdlog::error(u8"[fancy2d] [f2dRenderDevice::CreateGraphics2D] 创建2D渲染器失败");
			return false;
		}
		m_Graph2DLastBlendMode = BlendMode::AddAlpha;
		m_Graph2DBlendState = m_Graph2D->GetBlendState();
		m_Graph2DColorBlendState = m_Graph2D->GetColorBlendType();
		m_bRenderStarted = false;
		
		// 创建文字渲染器
		spdlog::info(u8"[fancy2d] 创建文本渲染器");
		if (FCYFAILED(m_pRenderer->CreateFontRenderer(nullptr, &m_FontRenderer)))
		{
			spdlog::error(u8"[fancy2d] [fcyRenderer::CreateFontRenderer] 创建文本渲染器失败");
			return false;
		}
		m_FontRenderer->SetZ(0.5f);
		
		// 创建图元渲染器
		spdlog::info(u8"[fancy2d] 创建平面几何渲染器");
		if (FCYFAILED(m_pRenderer->CreateGeometryRenderer(&m_GRenderer)))
		{
			spdlog::error(u8"[fancy2d] [fcyRenderer::CreateGeometryRenderer] 创建平面几何渲染器失败");
			return false;
		}
		
		// 创建3D渲染器
		spdlog::info(u8"[fancy2d] 创建后处理特效渲染器");
		if (FCYFAILED(m_pRenderDev->CreateGraphics3D(nullptr, &m_Graph3D)))
		{
			spdlog::error(u8"[fancy2d] [f2dRenderDevice::CreateGraphics3D] 创建后处理特效渲染器失败");
			return false;
		}
		m_Graph3DLastBlendMode = BlendMode::AddAlpha;
		m_Graph3DBlendState = m_Graph3D->GetBlendState();
		
		//创建鼠标输入
		spdlog::info(u8"[fancy2d] 创建DirectInput鼠标设备");
		m_pInputSys->CreateMouse(-1, false, &m_Mouse);
		if (!m_Mouse)
		{
			spdlog::error(u8"[fancy2d] [f2dInputSys::CreateMouse] 创建DirectInput鼠标设备失败");
		}
		// 创建键盘输入
		spdlog::info(u8"[fancy2d] 创建DirectInput键盘设备");
		m_pInputSys->CreateKeyboard(-1, false, &m_Keyboard);
		if (!m_Keyboard)
		{
			spdlog::error(u8"[fancy2d] [f2dInputSys::CreateKeyboard] 创建DirectInput键盘设备失败");
		}
		m_pInputSys->CreateDefaultKeyboard(-1, false, &m_Keyboard2);
		if (!m_Keyboard2)
		{
			spdlog::error(u8"[fancy2d] [f2dInputSys::CreateDefaultKeyboard] 创建DirectInput键盘设备失败");
		}
		
		//创建手柄输入
		/*
		try
		{
			m_DirectInput = std::make_unique<native::DirectInput>((ptrdiff_t)m_pMainWindow->GetHandle());
			{
				m_DirectInput->refresh(); // 这里因为窗口还没显示，所以应该会出现一个Aquire设备失败的错误信息，忽略即可
				uint32_t cnt = m_DirectInput->count();
				for (uint32_t i = 0; i < cnt; i += 1)
				{
					spdlog::info(u8"[luastg] 检测到{}控制器：{} {}",
						m_DirectInput->isXInputDevice(i) ? u8"XInput" : u8"DirectInput",
						fcyStringHelper::WideCharToMultiByte(m_DirectInput->getDeviceName(i)),
						fcyStringHelper::WideCharToMultiByte(m_DirectInput->getProductName(i))
					);
				}
				spdlog::info(u8"[luastg] 成功创建了{}个控制器", cnt);
			}
		}
		catch (const std::bad_alloc&)
		{
			spdlog::error(u8"[luastg] 无法为DirectInput分配内存");
		}
		//*/
		
		// 初始化ImGui
		imgui::bindEngine();
		// test
		slow::Graphic::_bindEngine((void*)m_pMainWindow->GetHandle());
		// 初始化自定义后处理特效
		//slow::effect::bindEngine();
		
		// 显示窗口
		m_pMainWindow->SetBorderType(F2DWINBORDERTYPE_FIXED);
		m_pMainWindow->SetClientRect(fcyRect(0.f, 0.f, m_OptionResolution.x, m_OptionResolution.y));
		m_pMainWindow->MoveToCenter();
		m_pMainWindow->SetVisiable(true);
		m_pMainWindow->HideMouse(!m_OptionCursor);
		resetKeyStatus(); // clear key status first
	}
	
	// 装载main脚本
	if (!OnLoadMainScriptAndFiles())
	{
		return false;
	}
	
	//////////////////////////////////////// 初始化完成
	m_iStatus = AppStatus::Initialized;
	spdlog::info(u8"[luastg] 初始化完成");
	
	//////////////////////////////////////// 调用GameInit
	if (!SafeCallGlobalFunction(LuaSTG::LuaEngine::G_CALLBACK_EngineInit)) {
		return false;
	}
	
	return true;
}

void AppFrame::Shutdown()LNOEXCEPT
{
	SafeCallGlobalFunction(LuaSTG::LuaEngine::G_CALLBACK_EngineStop);
	
	m_GameObjectPool = nullptr;
	spdlog::info(u8"[luastg] 清空对象池");

	m_ResourceMgr.ClearAllResource();
	spdlog::info(u8"[luastg] 清空所有游戏资源");
	
	// 卸载自定义后处理特效
	//slow::effect::unbindEngine();
	// test
	slow::Graphic::_unbindEngine();
	// 卸载ImGui
	imgui::unbindEngine();
	
	m_DirectInput = nullptr;
	m_Mouse = nullptr;
	m_Keyboard = nullptr;
	m_Keyboard2 = nullptr;
	m_Graph3D = nullptr;
	m_GRenderer = nullptr;
	m_FontRenderer = nullptr;
	m_Graph2D = nullptr;
	m_pInputSys = nullptr;
	m_pSoundSys = nullptr;
	m_pRenderDev = nullptr;
	m_pRenderer = nullptr;
	m_pMainWindow = nullptr;
	m_pEngine = nullptr;
	spdlog::info(u8"[fancy2d] 卸载所有组件");
	
	if (L)
	{
		lua_close(L);
		L = nullptr;
		spdlog::info(u8"[luastg] 关闭luajit引擎");
	}
	
	m_FileManager.UnloadAllArchive();
	spdlog::info(u8"[luastg] 卸载所有资源包");
	
	m_iStatus = AppStatus::Destroyed;
	spdlog::info(u8"[luastg] 引擎关闭");
}

void AppFrame::Run()LNOEXCEPT
{
	assert(m_iStatus == AppStatus::Initialized);
	spdlog::info(u8"[luastg] 开始更新&渲染循环");
	
	m_fFPS = 0.f;
#if (defined LDEVVERSION) || (defined LDEBUG)
	m_UpdateTimer = 0.f;
	m_RenderTimer = 0.f;
	m_PerformanceUpdateTimer = 0.f;
	m_PerformanceUpdateCounter = 0.f;
	m_FPSTotal = 0.f;
	m_ObjectTotal = 0.f;
	m_UpdateTimerTotal = 0.f;
	m_RenderTimerTotal = 0.f;
#endif
	
	m_pEngine->Run(F2DENGTHREADMODE_MULTITHREAD, m_OptionFPSLimit);
	
	spdlog::info(u8"[luastg] 结束更新&渲染循环");
}

#pragma endregion

#pragma region 游戏循环

fBool AppFrame::OnUpdate(fDouble ElapsedTime, f2dFPSController* pFPSController, f2dMsgPump* pMsgPump)
{
	#if (defined LDEVVERSION) || (defined LDEBUG)
	TimerScope tProfileScope(m_UpdateTimer);
	#endif
	
	m_fFPS = (float)pFPSController->GetFPS();
	pFPSController->SetLimitedFPS(m_OptionFPSLimit);
	
	m_LastKey = 0;
	
	// 处理消息
	f2dMsg tMsg;
	bool bResetDevice = false;
	bool bUpdateDevice = false;
	while (FCYOK(pMsgPump->GetMsg(&tMsg)))
	{
		switch (tMsg.Type)
		{
		case F2DMSG_WINDOW_ONCLOSE:
		{
			return false;  // 关闭窗口时结束循环
		}
		case F2DMSG_WINDOW_ONGETFOCUS:
		{
			resetKeyStatus(); // clear input status
			m_pInputSys->Reset(); // clear input status
			bResetDevice = true;
			if (m_LastInputTextEnable)
			{
				m_InputTextEnable = true;
			}
			
			lua_pushinteger(L, (lua_Integer)LuaSTG::LuaEngine::EngineEvent::WindowActive);
			lua_pushboolean(L, true);
			SafeCallGlobalFunctionB(LuaSTG::LuaEngine::G_CALLBACK_EngineEvent, 2, 0);
			
			if (!SafeCallGlobalFunction(LuaSTG::LuaEngine::G_CALLBACK_FocusGainFunc))
				return false;
			break;
		}
		case F2DMSG_WINDOW_ONLOSTFOCUS:
		{
			m_LastInputTextEnable = m_InputTextEnable;
			m_InputTextEnable = false;
			resetKeyStatus(); // clear input status
			m_pInputSys->Reset(); // clear input status
			bResetDevice = true;
			
			lua_pushinteger(L, (lua_Integer)LuaSTG::LuaEngine::EngineEvent::WindowActive);
			lua_pushboolean(L, false);
			SafeCallGlobalFunctionB(LuaSTG::LuaEngine::G_CALLBACK_EngineEvent, 2, 0);
			
			if (!SafeCallGlobalFunction(LuaSTG::LuaEngine::G_CALLBACK_FocusLoseFunc))
				return false;
			break;
		}
		case F2DMSG_WINDOW_ONRESIZE:
		{
			lua_pushinteger(L, (lua_Integer)LuaSTG::LuaEngine::EngineEvent::WindowResize);
			lua_pushinteger(L, (lua_Integer)tMsg.Param1);
			lua_pushinteger(L, (lua_Integer)tMsg.Param2);
			SafeCallGlobalFunctionB(LuaSTG::LuaEngine::G_CALLBACK_EngineEvent, 3, 0);
			break;
		}
		case F2DMSG_WINDOW_ONCHARINPUT:
		{
			if (m_InputTextEnable)
			{
				OnTextInputChar((fCharW)tMsg.Param1);
			}
			break;
		}
		case F2DMSG_WINDOW_ONKEYDOWN:
		{
			#ifdef USING_CTRL_ENTER_SWITCH
			// ctrl+enter全屏
			if (tMsg.Param1 == VK_RETURN && !m_KeyStateMap[VK_RETURN] && m_KeyStateMap[VK_CONTROL])  // 防止反复触发
			{
				ChangeVideoMode((int)m_OptionResolution.x, (int)m_OptionResolution.y, !m_OptionWindowed, m_OptionVsync);
			}
			#endif
			// text input
			if (m_InputTextEnable)
			{
				if (tMsg.Param1 == VK_BACK)
				{
					OnTextInputDeleteBack();
				}
				else if (tMsg.Param1 == VK_DELETE)
				{
					OnTextInputDeleteFront();
				}
				else if (tMsg.Param1 == 0x56 && !m_KeyStateMap[0x56] && m_KeyStateMap[VK_CONTROL]) // VK_RETURN + VK_V
				{
					OnTextInputPasting();
				}
			}
			// key
			if (0 < tMsg.Param1 && tMsg.Param1 < _countof(m_KeyStateMap))
			{
				m_LastKey = (fInt)tMsg.Param1;
				m_KeyStateMap[tMsg.Param1] = true;
			}
			break;
		}
		case F2DMSG_WINDOW_ONKEYUP:
		{
			if (m_LastKey == tMsg.Param1)
			{
				m_LastKey = 0;
			}
			if (0 < tMsg.Param1 && tMsg.Param1 < _countof(m_KeyStateMap))
			{
				m_KeyStateMap[tMsg.Param1] = false;
			}
			break;
		}
		case F2DMSG_WINDOW_ONMOUSEMOVE:
		{
			m_MousePosition_old.x = (float)static_cast<fInt>(tMsg.Param1);
			m_MousePosition_old.y = m_OptionResolution.y - (float)static_cast<fInt>(tMsg.Param2);  // ! 潜在大小不匹配问题
			m_MousePosition.x = (float)static_cast<fInt>(tMsg.Param1);
			m_MousePosition.y = (float)static_cast<fInt>(tMsg.Param2);
			break;
		}
		case F2DMSG_SYSTEM_ON_DEVICE_CHANGE:
		{
			bUpdateDevice = true;
			break;
		}
		default:
			break;
		}
	}
	
	if (m_DirectInput.get())
	{
		if (bResetDevice) m_DirectInput->reset();
		if (bUpdateDevice) m_DirectInput->refresh();
	}
	
	if (!slow::Graphic::_update())
		return false;
	
	// 执行帧函数
	if (!SafeCallGlobalFunction(LuaSTG::LuaEngine::G_CALLBACK_EngineUpdate, 1))
		return false;
	bool tAbort = lua_toboolean(L, -1) == 0 ? false : true;
	lua_pop(L, 1);
	
	#if (defined LDEVVERSION) || (defined LDEBUG)
	// 刷新性能计数器
	m_PerformanceUpdateTimer += static_cast<float>(ElapsedTime);
	m_PerformanceUpdateCounter += 1.f;
	m_FPSTotal += static_cast<float>(m_fFPS);
	m_ObjectTotal += (float)m_GameObjectPool->GetObjectCount();
	m_UpdateTimerTotal += m_UpdateTimer;
	m_RenderTimerTotal += m_RenderTimer;
	if (m_PerformanceUpdateTimer > 0.05f) // 每隔1/20秒刷新一次计数器
	{
		//m_FPSTotal / m_PerformanceUpdateCounter,
		//m_ObjectTotal / m_PerformanceUpdateCounter,
		//m_UpdateTimerTotal / m_PerformanceUpdateCounter,
		//m_RenderTimerTotal / m_PerformanceUpdateCounter
		m_PerformanceUpdateTimer = 0.f;
		m_PerformanceUpdateCounter = 0.f;
		m_FPSTotal = 0.f;
		m_ObjectTotal = 0.f;
		m_UpdateTimerTotal = 0.f;
		m_RenderTimerTotal = 0.f;
	}
	#endif
	
	return !tAbort;
}

fBool AppFrame::OnRender(fDouble ElapsedTime, f2dFPSController* pFPSController)
{
	#if (defined LDEVVERSION) || (defined LDEBUG)
	TimerScope tProfileScope(m_RenderTimer);
	#endif
	
	if (slow::Graphic::_draw())
	{
		return false;
	}
	
	m_pRenderDev->Clear();
	
	// 执行渲染函数
	m_bRenderStarted = true;
	
	if (!SafeCallGlobalFunction(LuaSTG::LuaEngine::G_CALLBACK_EngineDraw))
		m_pEngine->Abort();
	if (!m_stRenderTargetStack.empty())
	{
		spdlog::error(u8"[luastg] [AppFrame::OnRender] 渲染结束时RenderTarget栈不为空，可能缺少对lstg.PopRenderTarget的调用");
		while (!m_stRenderTargetStack.empty())
			PopRenderTarget();
	}
	m_bRenderStarted = false;
	
	return true;
}

#pragma endregion
