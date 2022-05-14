﻿#include "AppFrame.h"
#include "ResourceParticle.hpp"
#include "LConfig.h"

#ifdef max
#undef max
#endif
#ifdef min
#undef min
#endif

using namespace std;

namespace LuaSTGPlus
{
	fcyMemPool<sizeof(LuaSTGPlus::ResParticle::ParticlePool)> LuaSTGPlus::ResParticle::s_MemoryPool(512);  // 预分配512个对象

	bool ResParticle::ParticleSystemResourceInfo::LoadFromMemory(void const* data, size_t size)
	{
		if (size != sizeof(hgeParticleSystemInfo))
		{
			return false;
		}
		std::memcpy(this, data, sizeof(hgeParticleSystemInfo));
		return LoadFromInfo();
	}
	bool ResParticle::ParticleSystemResourceInfo::LoadFromInfo()
	{
		tParticleSystemInfo.iBlendInfo = (tParticleSystemInfo.iBlendInfo >> 16) & 0x00000003u;
		eBlendMode = BlendMode::MulAlpha;
		if (tParticleSystemInfo.iBlendInfo & 0x1)  // ADD
		{
			if (tParticleSystemInfo.iBlendInfo & 0x2)  // ALPHA
				eBlendMode = BlendMode::AddAlpha;
			else
				eBlendMode = BlendMode::AddAdd;
		}
		else  // MUL
		{
			if (tParticleSystemInfo.iBlendInfo & 0x2)  // ALPHA
				eBlendMode = BlendMode::MulAlpha;
			else
				eBlendMode = BlendMode::MulAdd;
		}
		return true;
	}

	ResParticle::ResParticle(const char* name, const hgeParticleSystemInfo& pinfo, fcyRefPointer<f2dSprite> sprite, double a, double b, bool rect)
		: Resource(ResourceType::Particle, name)
		, m_HalfSizeX(a)
		, m_HalfSizeY(b)
		, m_bRectangle(rect)
	{
		m_Info.tParticleSystemInfo = pinfo;
		m_Info.pSprite = sprite;
		m_Info.LoadFromInfo(); // 加载混合模式
	}

	ResParticle::ParticlePool* ResParticle::AllocInstance() noexcept
	{
		// ！ 警告：潜在bad_alloc导致错误，暂时不予处理
		return new(s_MemoryPool.Alloc()) ParticlePool(this);
	}
	void ResParticle::FreeInstance(ResParticle::ParticlePool* p) noexcept
	{
		p->~ParticlePool();
		s_MemoryPool.Free(p);
	}

	ResParticle::ParticlePool::ParticlePool(fcyRefPointer<ResParticle> ref)
	{
		m_Res = ref;
		m_Info = ref->m_Info;
	}
	size_t ResParticle::ParticlePool::GetAliveCount() const noexcept { return m_iAlive; }
	BlendMode ResParticle::ParticlePool::GetBlendMode() const noexcept { return m_Info.eBlendMode; }
	void ResParticle::ParticlePool::SetBlendMode(BlendMode m) noexcept { m_Info.eBlendMode = m; }
	fcyColor ResParticle::ParticlePool::GetVertexColor() const noexcept
	{
		return fcyColor(
			(fInt)std::clamp(m_Info.colVertexColor[3] * 255.0f, 0.0f, 255.0f),
			(fInt)std::clamp(m_Info.colVertexColor[0] * 255.0f, 0.0f, 255.0f),
			(fInt)std::clamp(m_Info.colVertexColor[1] * 255.0f, 0.0f, 255.0f),
			(fInt)std::clamp(m_Info.colVertexColor[2] * 255.0f, 0.0f, 255.0f)
		);
	}
	void ResParticle::ParticlePool::SetVertexColor(fcyColor c) noexcept
	{
		m_Info.colVertexColor[0] = (float)c.r / 255.0f;
		m_Info.colVertexColor[1] = (float)c.g / 255.0f;
		m_Info.colVertexColor[2] = (float)c.b / 255.0f;
		m_Info.colVertexColor[3] = (float)c.a / 255.0f;
	}
	int ResParticle::ParticlePool::GetEmission() const noexcept { return m_Info.tParticleSystemInfo.nEmission; }
	void ResParticle::ParticlePool::SetEmission(int e) noexcept { m_Info.tParticleSystemInfo.nEmission = e; }
	fuInt ResParticle::ParticlePool::GetSeed() const noexcept
	{
		return m_Random.GetRandSeed();
	}
	void ResParticle::ParticlePool::SetSeed(fuInt seed) noexcept
	{
		m_Random.SetSeed(seed);
	}
	bool ResParticle::ParticlePool::IsActived() const noexcept { return m_iStatus == Status::Alive; }
	void ResParticle::ParticlePool::SetActive(bool v) noexcept
	{
		if (v)
		{
			m_iStatus = Status::Alive;
			m_fAge = 0.f;
		}
		else
		{
			m_iStatus = Status::Sleep;
		}
	}
	void ResParticle::ParticlePool::SetCenter(fcyVec2 pos) noexcept
	{
		if (m_iStatus == Status::Alive)
			m_vPrevCenter = m_vCenter;
		else
			m_vPrevCenter = pos;
		m_vCenter = pos;
	}
	fcyVec2 ResParticle::ParticlePool::GetCenter() const noexcept
	{
		return m_vCenter;
	}
	void ResParticle::ParticlePool::SetRotation(float r) noexcept { m_fDirection = r; }
	float ResParticle::ParticlePool::GetRotation() const noexcept { return m_fDirection; }
	void ResParticle::ParticlePool::Update(float delta)
	{
		hgeParticleSystemInfo const& pInfo = m_Info.tParticleSystemInfo;

		if (m_iStatus == Status::Alive)
		{
			m_fAge += delta;
			if (m_fAge >= pInfo.fLifetime && pInfo.fLifetime >= 0.0f)
			{
				m_iStatus = Status::Sleep;
			}
		}

		// 更新所有粒子
		for (size_t i = 0; i < m_iAlive; i += 1)
		{
			hgeParticle& tInst = m_ParticlePool[i];
			tInst.fAge += delta;
			if (tInst.fAge >= tInst.fTerminalAge)
			{
				m_iAlive -= 1;
				if (i < m_iAlive)
				{
					// 需要拷贝最后一个粒子到当前位置
					std::memcpy(&tInst, &m_ParticlePool[m_iAlive], sizeof(hgeParticle));
				}
				// 回溯索引
				i -= 1;
				continue;
			}

			// 计算线加速度和切向加速度
			fcyVec2 vecAccel = (tInst.vecLocation - m_vCenter).GetNormalize();
			fcyVec2 vecAccel2 = vecAccel;
			vecAccel *= tInst.fRadialAccel;
			// 相当于旋转向量 vecAccel2.Rotate(M_PI_2);
			std::swap(vecAccel2.x, vecAccel2.y);
			vecAccel2.x = -vecAccel2.x;
			vecAccel2 *= tInst.fTangentialAccel;

			// 计算速度
			tInst.vecVelocity += (vecAccel + vecAccel2) * delta;
			tInst.vecVelocity.y += tInst.fGravity * delta;

			// 计算位置
			tInst.vecLocation += tInst.vecVelocity * delta;

			// 计算自旋和大小
			tInst.fSpin += tInst.fSpinDelta * delta;
			tInst.fSize += tInst.fSizeDelta * delta;
			tInst.colColor[0] += tInst.colColorDelta[0] * delta;
			tInst.colColor[1] += tInst.colColorDelta[1] * delta;
			tInst.colColor[2] += tInst.colColorDelta[2] * delta;
			tInst.colColor[3] += tInst.colColorDelta[3] * delta;
		}

		// 产生新的粒子
		if (m_iStatus == Status::Alive)
		{
			float const fParticlesNeeded = (float)pInfo.nEmission * delta + m_fEmissionResidue;
			fuInt const nParticlesCreated = (fuInt)fParticlesNeeded;
			m_fEmissionResidue = fParticlesNeeded - (float)nParticlesCreated;

			for (fuInt i = 0; i < nParticlesCreated; ++i)
			{
				if (m_iAlive >= m_ParticlePool.size())
					break;

				hgeParticle& tInst = m_ParticlePool[m_iAlive];
				m_iAlive += 1;

				tInst.fAge = 0.0f;
				tInst.fTerminalAge = m_Random.GetRandFloat(pInfo.fParticleLifeMin, pInfo.fParticleLifeMax);

				tInst.vecLocation = m_vPrevCenter + (m_vCenter - m_vPrevCenter) * m_Random.GetRandFloat(0.0f, 1.0f);
				tInst.vecLocation.x += m_Random.GetRandFloat(-2.0f, 2.0f);
				tInst.vecLocation.y += m_Random.GetRandFloat(-2.0f, 2.0f);

				// TODO: 删除
				//float ang = /* pInfo.fDirection */ (m_fRotation - (float)LPI_HALF) - (float)LPI_HALF + s_ParticleRandomizer.GetRandFloat(0, pInfo.fSpread) - pInfo.fSpread / 2.0f;
				
				// 来自 HGE 的原始代码，但是似乎 HGE 的坐标系 y 轴是向下的，直接拿来用并不可行
				//float ang = pInfo.fDirection - (float)LPI_HALF + m_Random.GetRandFloat(0, pInfo.fSpread) - pInfo.fSpread / 2.0f;
				// 修改后的正确的代码应该是这个
				float ang = -pInfo.fDirection + (float)LPI_HALF + m_Random.GetRandFloat(0, pInfo.fSpread) - pInfo.fSpread / 2.0f;
				if (pInfo.bRelative)
				{
					ang += (m_vPrevCenter - m_vCenter).CalcuAngle() + (float)LPI_HALF;
				}
				// 此外，我们还有自己的旋转量
				ang += m_fDirection;
				
				tInst.vecVelocity.x = std::cos(ang);
				tInst.vecVelocity.y = std::sin(ang);
				tInst.vecVelocity *= m_Random.GetRandFloat(pInfo.fSpeedMin, pInfo.fSpeedMax);

				tInst.fGravity = m_Random.GetRandFloat(pInfo.fGravityMin, pInfo.fGravityMax);
				tInst.fRadialAccel = m_Random.GetRandFloat(pInfo.fRadialAccelMin, pInfo.fRadialAccelMax);
				tInst.fTangentialAccel = m_Random.GetRandFloat(pInfo.fTangentialAccelMin, pInfo.fTangentialAccelMax);

				tInst.fSize = m_Random.GetRandFloat(pInfo.fSizeStart, pInfo.fSizeStart + (pInfo.fSizeEnd - pInfo.fSizeStart) * pInfo.fSizeVar);
				tInst.fSizeDelta = (pInfo.fSizeEnd - tInst.fSize) / tInst.fTerminalAge;

				// TODO: 删除
				//tInst.fSpin = /* pInfo.fSpinStart */ m_fRotation + s_ParticleRandomizer.GetRandFloat(0, pInfo.fSpinEnd) - pInfo.fSpinEnd / 2.0f;
				//tInst.fSpinDelta = pInfo.fSpinVar;

				tInst.fSpin = m_Random.GetRandFloat(pInfo.fSpinStart, pInfo.fSpinStart + (pInfo.fSpinEnd - pInfo.fSpinStart) * pInfo.fSpinVar);
				tInst.fSpinDelta = (pInfo.fSpinEnd - tInst.fSpin) / tInst.fTerminalAge;

				tInst.colColor[0] = m_Random.GetRandFloat(pInfo.colColorStart[0], pInfo.colColorStart[0] + (pInfo.colColorEnd[0] - pInfo.colColorStart[0]) * pInfo.fColorVar);
				tInst.colColor[1] = m_Random.GetRandFloat(pInfo.colColorStart[1], pInfo.colColorStart[1] + (pInfo.colColorEnd[1] - pInfo.colColorStart[1]) * pInfo.fColorVar);
				tInst.colColor[2] = m_Random.GetRandFloat(pInfo.colColorStart[2], pInfo.colColorStart[2] + (pInfo.colColorEnd[2] - pInfo.colColorStart[2]) * pInfo.fColorVar);
				tInst.colColor[3] = m_Random.GetRandFloat(pInfo.colColorStart[3], pInfo.colColorStart[3] + (pInfo.colColorEnd[3] - pInfo.colColorStart[3]) * pInfo.fAlphaVar);

				tInst.colColorDelta[0] = (pInfo.colColorEnd[0] - tInst.colColor[0]) / tInst.fTerminalAge;
				tInst.colColorDelta[1] = (pInfo.colColorEnd[1] - tInst.colColor[1]) / tInst.fTerminalAge;
				tInst.colColorDelta[2] = (pInfo.colColorEnd[2] - tInst.colColor[2]) / tInst.fTerminalAge;
				tInst.colColorDelta[3] = (pInfo.colColorEnd[3] - tInst.colColor[3]) / tInst.fTerminalAge;
			}
		}

		m_vPrevCenter = m_vCenter;
	}
	void ResParticle::ParticlePool::Render(f2dGraphics2D* graph, float scaleX, float scaleY)
	{
		f2dSprite* pSprite = *m_Info.pSprite;
		fcyColor tOrgColor[4] = {};
		pSprite->GetColor(tOrgColor);
		hgeParticleSystemInfo const& pInfo = m_Info.tParticleSystemInfo;
		fcyColor const tVertexColor = GetVertexColor();
		for (size_t i = 0; i < m_iAlive; i += 1)
		{
			hgeParticle const& pInst = m_ParticlePool[i];
			if (pInfo.colColorStart[0] < 0) // r < 0
			{
				pSprite->SetColor(fcyColor(
					(fByte)std::clamp(pInst.colColor[3] * (float)tVertexColor.a, 0.0f, 255.0f),
					tVertexColor.r,
					tVertexColor.g,
					tVertexColor.b
				));
			}
			else
			{
				pSprite->SetColor(fcyColor(
					(fInt)std::clamp(pInst.colColor[3] * (float)tVertexColor.a, 0.0f, 255.0f),
					(fInt)std::clamp(pInst.colColor[0] * (float)tVertexColor.r, 0.0f, 255.0f),
					(fInt)std::clamp(pInst.colColor[1] * (float)tVertexColor.g, 0.0f, 255.0f),
					(fInt)std::clamp(pInst.colColor[2] * (float)tVertexColor.b, 0.0f, 255.0f)
				));
			}
			pSprite->Draw2(graph, pInst.vecLocation, fcyVec2(scaleX * pInst.fSize, scaleY * pInst.fSize), pInst.fSpin);
		}
		pSprite->SetColor(tOrgColor);
	}
}
