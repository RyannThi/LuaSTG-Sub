#pragma once
#include "Core/Object.hpp"
#include "Core/Graphics/Device.hpp"

// Buffer
namespace Core::Graphics::Direct3D11 {
	class Device;

	class Buffer final
		: public Object<IBuffer>
		, public IDeviceEventListener {
	public:
		// IDeviceEventListener

		void onDeviceCreate() override;
		void onDeviceDestroy() override;

		// IBuffer

		bool map(uint32_t size_in_bytes, bool discard, void** out_pointer) override;
		bool unmap() override;

		// Buffer

		Buffer();
		Buffer(Buffer const&) = delete;
		Buffer(Buffer&&) = delete;
		Buffer& operator=(Buffer const&) = delete;
		Buffer& operator=(Buffer&&) = delete;
		~Buffer();

		bool initialize(Device* device, uint8_t type, uint32_t size_in_bytes);
		bool createResources();

	private:
		ScopeObject<Device> m_device;
		wil::com_ptr_nothrow<ID3D11Buffer> m_buffer;
		uint32_t m_size_in_bytes{};
		uint8_t m_type{}; // 0-unknown, 1-vertex, 2-index
		bool m_initialized{ false };
	};
}
