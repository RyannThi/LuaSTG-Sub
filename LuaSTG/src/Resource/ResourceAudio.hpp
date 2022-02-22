﻿#pragma once
#include "ResourceBase.hpp"
#include "f2dSoundSys.h"

namespace LuaSTGPlus {
	// 音效
	class ResSound :
		public Resource
	{
	private:
		fcyRefPointer<f2dSoundBuffer> m_pBuffer;
		int m_status;//0停止1暂停2播放
	public:
		void Play(float vol, float pan)
		{
			m_pBuffer->Stop();
			
			m_pBuffer->SetVolume(vol);
			m_pBuffer->SetPan(pan);

			m_pBuffer->Play();
			m_status = 2;
		}

		void Resume()
		{
			m_pBuffer->Play();
			m_status = 2;
		}

		void Pause()
		{
			m_pBuffer->Pause();
			m_status = 1;
		}

		void Stop()
		{
			m_pBuffer->Stop();
			m_status = 0;
		}

		bool IsPlaying()
		{
			return m_pBuffer->IsPlaying();
		}

		bool IsStopped()
		{
			return !IsPlaying() && m_status != 1;
		}

		bool SetSpeed(float speed) {
			return m_pBuffer->SetFrequency(speed) == FCYERR_OK;
		}

		float GetSpeed() {
			return m_pBuffer->GetFrequency();
		}
	public:
		ResSound(const char* name, fcyRefPointer<f2dSoundBuffer> buffer) : Resource(ResourceType::SoundEffect, name) {
			m_pBuffer = buffer;
		}
	};

	// 背景音乐
	class ResMusic :
		public Resource
	{
	public:
		// 对SoundDecoder作一个包装来保持BGM循环
		// 使用该Wrapper之后SoundBuffer的播放参数（位置）将没有意义
		// ! 从fancystg（已坑）中抽取的上古时期代码
		class BGMWrapper :
			public fcyRefObjImpl<f2dSoundDecoder>
		{
		protected:
			fcyRefPointer<f2dSoundDecoder> m_pDecoder;
			bool m_bIsLoop = true;
			// 转到采样为单位
			fuInt m_TotalSample;
			fuInt m_pLoopStartSample;
			fuInt m_pLoopEndSample; // 监视哨，=EndSample+1
		public:
			// 直接返回原始参数
			fuInt GetBufferSize() { return m_pDecoder->GetBufferSize(); }
			fuInt GetAvgBytesPerSec() { return m_pDecoder->GetAvgBytesPerSec(); }
			fuShort GetBlockAlign() { return m_pDecoder->GetBlockAlign(); }
			fuShort GetChannelCount() { return m_pDecoder->GetChannelCount(); }
			fuInt GetSamplesPerSec() { return m_pDecoder->GetSamplesPerSec(); }
			fuShort GetFormatTag() { return m_pDecoder->GetFormatTag(); }
			fuShort GetBitsPerSample() { return m_pDecoder->GetBitsPerSample(); }

			// 不作任何处理
			fLen GetPosition() { return m_pDecoder->GetPosition(); }
			fResult SetPosition(F2DSEEKORIGIN Origin, fInt Offset) { return m_pDecoder->SetPosition(Origin, Offset); }

			// 对Read作处理
			fResult Read(fData pBuffer, fuInt SizeToRead, fuInt* pSizeRead);
			
			void SetLoop(bool v) { m_bIsLoop = v ;}
		public:
			BGMWrapper(fcyRefPointer<f2dSoundDecoder> pOrg, fDouble LoopStart, fDouble LoopEnd);
		};
	private:
		fcyRefPointer<BGMWrapper> m_pDecoder;
		fcyRefPointer<f2dSoundBuffer> m_pBuffer;
		int m_status;//0停止1暂停2播放
	public:
		f2dSoundBuffer* GetAudioSource() { return *m_pBuffer; }

		void Play(float vol, double position)
		{
			m_pBuffer->Stop();
			m_pBuffer->SetTime(position);

			m_pBuffer->SetVolume(vol);

			m_pBuffer->Play();
			m_status = 2;
		}

		void Stop()
		{
			m_pBuffer->Stop();
			m_status = 0;
		}

		void Pause()
		{
			m_pBuffer->Pause();
			m_status = 1;
		}

		void Resume()
		{
			m_pBuffer->Play();
			m_status = 2;
		}

		bool IsPlaying()
		{
			return m_pBuffer->IsPlaying();
		}

		bool IsPaused()
		{
			return m_status == 1;
		}

		bool IsStopped()
		{
			return !IsPlaying() && m_pBuffer->GetTotalTime() == 0.0;
		}

		void SetVolume(float v)
		{
			m_pBuffer->SetVolume(v);
		}

		float GetVolume()
		{
			return m_pBuffer->GetVolume();
		}

		bool SetSpeed(float speed) {
			return m_pBuffer->SetFrequency(speed) == FCYERR_OK;
		}

		float GetSpeed() {
			return m_pBuffer->GetFrequency();
		}
		
		void SetLoop(bool v)
		{
			m_pDecoder->SetLoop(v);
		}
	public:
		ResMusic(const char* name, fcyRefPointer<BGMWrapper> decoder, fcyRefPointer<f2dSoundBuffer> buffer) : Resource(ResourceType::Music, name) {
			m_pDecoder = decoder;
			m_pBuffer = buffer;
			m_status = 0;
		}
	};
}
