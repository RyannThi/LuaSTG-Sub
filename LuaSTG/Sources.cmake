
set(LUASTG_ENGINE_CORE_SOURCES
    Core/Type.hpp

    Core/i18n.hpp
    Core/i18n.cpp
    Core/framework.hpp
    Core/framework.cpp
    Core/Object.hpp

    Core/FileManager.hpp
    Core/FileManager.cpp

    Core/Graphics/Window.hpp
    Core/Graphics/Window_Win32.hpp
    Core/Graphics/Window_Win32.cpp
    Core/Graphics/Format.hpp
    Core/Graphics/Format_D3D11.hpp
    Core/Graphics/Device.hpp
    Core/Graphics/Device_D3D11.hpp
    Core/Graphics/Device_D3D11.cpp
    Core/Graphics/SwapChain.hpp
    Core/Graphics/SwapChain_D3D11.hpp
    Core/Graphics/SwapChain_D3D11.cpp
    Core/Graphics/Renderer.hpp
    Core/Graphics/Renderer_D3D11.hpp
    Core/Graphics/Renderer_D3D11.cpp
    Core/Graphics/Renderer_Shader_D3D11.cpp
    Core/Graphics/Model_D3D11.hpp
    Core/Graphics/Model_D3D11.cpp
    Core/Graphics/Model_Shader_D3D11.cpp
    Core/Graphics/Sprite.hpp
    Core/Graphics/Sprite_D3D11.hpp
    Core/Graphics/Sprite_D3D11.cpp
    Core/Graphics/Font.hpp
    Core/Graphics/Font_D3D11.hpp
    Core/Graphics/Font_D3D11.cpp
    Core/Graphics/DearImGui_Win32_D3D11.hpp
    Core/Graphics/DearImGui_Win32_D3D11.cpp
    Core/ApplicationModel.hpp
    Core/ApplicationModel_Win32.hpp
    Core/ApplicationModel_Win32.cpp

    Core/Audio/Decoder.hpp
    Core/Audio/Decoder_VorbisOGG.cpp
    Core/Audio/Decoder_VorbisOGG.hpp
    Core/Audio/Decoder_WAV.cpp
    Core/Audio/Decoder_WAV.hpp
    Core/Audio/Decoder_ALL.cpp
)
source_group(TREE ${CMAKE_CURRENT_LIST_DIR} FILES ${LUASTG_ENGINE_CORE_SOURCES})

set(LUASTG_ENGINE_SOURCES
    pch/pch.h
    pch/pch.cpp
    
    cpp/CircularQueue.hpp
    cpp/Dictionary.hpp
    cpp/fixed_object_pool.hpp
    
    src/LuaWrapper/lua_xinput.hpp
    src/LuaWrapper/lua_xinput.cpp
    src/LuaWrapper/lua_utility.hpp
    src/LuaWrapper/lua_luastg_hash.cpp
    src/LuaWrapper/lua_luastg_hash.hpp
    src/LuaWrapper/LuaAppFrame.hpp
    src/LuaWrapper/LuaCustomLoader.cpp
    src/LuaWrapper/LuaCustomLoader.hpp
    src/LuaWrapper/LuaInternalSource.cpp
    src/LuaWrapper/LuaInternalSource.hpp
    src/LuaWrapper/LuaTableToOption.cpp
    src/LuaWrapper/LuaTableToOption.hpp
    src/LuaWrapper/LuaWrapper.cpp
    src/LuaWrapper/LuaWrapper.hpp
    src/LuaWrapper/LuaWrapperMisc.hpp
    src/LuaWrapper/LW_Archive.cpp
    src/LuaWrapper/LW_Audio.cpp
    src/LuaWrapper/LW_BentLaser.cpp
    src/LuaWrapper/LW_Color.cpp
    src/LuaWrapper/LW_DInput.cpp
    src/LuaWrapper/LW_FileManager.cpp
    src/LuaWrapper/LW_Input.cpp
    src/LuaWrapper/LW_IO_BinaryReader.cpp
    src/LuaWrapper/LW_IO_BinaryWriter.cpp
    src/LuaWrapper/LW_IO_Steam.cpp
    src/LuaWrapper/LW_IO.cpp
    src/LuaWrapper/LW_LuaSTG.cpp
    src/LuaWrapper/LW_ParticleSystem.cpp
    src/LuaWrapper/LW_Randomizer.cpp
    src/LuaWrapper/LW_Render.cpp
    src/LuaWrapper/LW_Renderer.cpp
    src/LuaWrapper/LW_StopWatch.cpp
    src/LuaWrapper/LW_Window.cpp
    src/LuaWrapper/LW_ResourceMgr.cpp
    src/LuaWrapper/LW_Platform.cpp
    src/LuaWrapper/LW_GameObjectManager.cpp
    
    src/Resource/ResourceDebug.cpp
    src/Resource/ResourceAnimation.cpp
    src/Resource/ResourceAnimation.hpp
    src/Resource/ResourceAudio.cpp
    src/Resource/ResourceAudio.hpp
    src/Resource/ResourceBase.hpp
    src/Resource/ResourceFont.cpp
    src/Resource/ResourceFont.hpp
    src/Resource/ResourceFX.cpp
    src/Resource/ResourceFX.hpp
    src/Resource/ResourceMgr.cpp
    src/Resource/ResourceMgr.h
    src/Resource/ResourceModel.hpp
    src/Resource/ResourceParticle.cpp
    src/Resource/ResourceParticle.hpp
    src/Resource/ResourcePassword.hpp
    src/Resource/ResourcePool.cpp
    src/Resource/ResourceSprite.hpp
    src/Resource/ResourceTexture.hpp
    src/Resource/ResourceTexture.cpp
    
    src/SteamAPI/SteamAPI.cpp
    src/SteamAPI/SteamAPI.hpp
    
    src/AdapterPolicy.hpp
    src/AdapterPolicy.cpp
    src/AppFrame.cpp
    src/AppFrame.h
    src/AppFrameFontRenderer.cpp
    src/AppFrameInput.cpp
    src/AppFrameLua.cpp
    src/AppFrameRender.cpp
    src/AppFrameRenderEx.cpp
    src/GameObject.cpp
    src/GameObject.hpp
    src/GameObjectBentLaser.cpp
    src/GameObjectBentLaser.hpp
    src/GameObjectClass.cpp
    src/GameObjectClass.hpp
    src/GameObjectPool.cpp
    src/GameObjectPool.h
    src/Global.h
    src/ImGuiExtension.cpp
    src/ImGuiExtension.h
    src/LConfig.h
    src/LLogger.cpp
    src/LLogger.hpp
    src/LMathConstant.hpp
    src/Main.cpp
    src/ScopeObject.cpp
    src/Utility.h
    
    Core/Type.hpp
    Core/InitializeConfigure.hpp
    Core/InitializeConfigure.cpp
    
    src/LuaSTG.manifest
)
source_group(TREE ${CMAKE_CURRENT_LIST_DIR} FILES ${LUASTG_ENGINE_SOURCES})
