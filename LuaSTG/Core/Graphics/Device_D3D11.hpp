﻿#pragma once
#include "Core/Object.hpp"
#include "Core/Graphics/Device.hpp"

namespace LuaSTG::Core::Graphics
{
	class Device_D3D11 : public Object<IDevice>
	{
	private:
		// DXGI

		HMODULE dxgi_dll{ NULL };
		decltype(::CreateDXGIFactory1)* dxgi_api_CreateDXGIFactory1{ NULL };
		decltype(::CreateDXGIFactory2)* dxgi_api_CreateDXGIFactory2{ NULL };

		Microsoft::WRL::ComPtr<IDXGIFactory1> dxgi_factory;
		Microsoft::WRL::ComPtr<IDXGIFactory2> dxgi_factory2;
		Microsoft::WRL::ComPtr<IDXGIAdapter1> dxgi_adapter;

		std::string preferred_adapter_name;

		std::string dxgi_adapter_name;
		std::vector<std::string> dxgi_adapter_names;

		DWORD dwm_acceleration_level{ 0 };
		BOOL dxgi_support_flip_model{ FALSE };
		BOOL dxgi_support_low_latency{ FALSE };
		BOOL dxgi_support_flip_model2{ FALSE };
		BOOL dxgi_support_tearing{ FALSE };

		// Direct3D

		D3D_FEATURE_LEVEL d3d_feature_level{ D3D_FEATURE_LEVEL_10_0 };

		// Direct3D 11

		HMODULE d3d11_dll{ NULL };
		decltype(::D3D11CreateDevice)* d3d11_api_D3D11CreateDevice{ NULL };

		Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device;
		Microsoft::WRL::ComPtr<ID3D11Device1> d3d11_device1;
		Microsoft::WRL::ComPtr<ID3D11DeviceContext> d3d11_devctx;
		Microsoft::WRL::ComPtr<ID3D11DeviceContext1> d3d11_devctx1;

	public:
		// Get API

		IDXGIFactory1* GetDXGIFactory1() { return dxgi_factory.Get(); }
		IDXGIFactory2* GetDXGIFactory2() { return dxgi_factory2.Get(); }
		IDXGIAdapter1* GetDXGIAdapter1() { return dxgi_adapter.Get(); }

		void SetPreferredAdapter(std::string_view name) { preferred_adapter_name = name; }
		std::string_view GetAdapterName() { return dxgi_adapter_name; }
		std::vector<std::string_view> GetAdapterNameArray()
		{
			std::vector<std::string_view> arr(dxgi_adapter_names.size());
			for (auto const& s : dxgi_adapter_names) arr.emplace_back(s);
			return std::move(arr);
		}

		D3D_FEATURE_LEVEL GetD3DFeatureLevel() { return d3d_feature_level; }

		ID3D11Device* GetD3D11Device() { return d3d11_device.Get(); }
		ID3D11Device1* GetD3D11Device1() { return d3d11_device1.Get(); }
		ID3D11DeviceContext* GetD3D11DeviceContext() { return d3d11_devctx.Get(); }
		ID3D11DeviceContext1* GetD3D11DeviceContext1() { return d3d11_devctx1.Get(); }

	public:
		bool selectAdapter();
		bool createDXGI();
		void destroyDXGI();
		bool createD3D11();
		void destroyD3D11();

	public:
		Device_D3D11(std::string_view const& prefered_gpu = "");
		~Device_D3D11();
	};
}
