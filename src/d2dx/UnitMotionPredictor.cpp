/*
	This file is part of D2DX.

	Copyright (C) 2021  Bolrog

	D2DX is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	D2DX is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with D2DX.  If not, see <https://www.gnu.org/licenses/>.
*/
#include "pch.h"
#include "UnitMotionPredictor.h"
#include "Vertex.h"

using namespace d2dx;
using namespace DirectX;

const std::int32_t D2_FRAME_LENGTH = (1 << 16) / 25;
const float FLOAT_TO_FIXED_MUL = static_cast<float>(1 << 16);
const float FIXED_TO_FLOAT_MUL = 1.f / FLOAT_TO_FIXED_MUL;

float fixedToFloat(int32_t x) {
	return static_cast<float>(x) * FIXED_TO_FLOAT_MUL;
}

OffsetF fixedToFloat(Offset x) {
	return OffsetF(
		static_cast<float>(x.x) * FIXED_TO_FLOAT_MUL,
		static_cast<float>(x.y) * FIXED_TO_FLOAT_MUL
	);
}

Offset floatToFixed(OffsetF x) {
	return Offset(
		static_cast<int32_t>(x.x * FLOAT_TO_FIXED_MUL),
		static_cast<int32_t>(x.y * FLOAT_TO_FIXED_MUL)
	);
}

_Use_decl_annotations_
UnitMotionPredictor::UnitMotionPredictor(
	const std::shared_ptr<IGameHelper>& gameHelper) :
	_gameHelper{ gameHelper }
{
	_units.reserve(1024);
	_prevUnits.reserve(1024);
	_shadows.reserve(1024);
}

_Use_decl_annotations_
Offset UnitMotionPredictor::GetOffset(
	D2::UnitAny const* unit,
	Offset screenPos)
{
	auto info = _gameHelper->GetUnitInfo(unit);
	auto prev = std::lower_bound(_prevUnits.begin(), _prevUnits.end(), unit, [&](auto const& x, auto const& y) { return x.unit < y; });
	if (prev == _prevUnits.end() || prev->id != info.id || prev->type != info.type) {
		if (!_update) {
			_update = true;
			_sinceLastUpdate = 0;


			for (auto& pred : _units) {
				pred.predictedPos = pred.actualPos;
				pred.basePos = pred.lastRenderedPos;
			}
		}
		_units.push_back(Unit(unit, info, screenPos));
		return { 0, 0 };
	}
	if (prev->nextIdx != -1) {
		return _units[prev->nextIdx].lastRenderedScreenOffset;
	}

	auto prevUnit = *prev;
	prev->nextIdx = _units.size();
	prevUnit.screenPos = screenPos;
	if (!_update && prevUnit.actualPos != info.pos) {
		_update = true;
		_sinceLastUpdate = 0;

		for (auto& pred : _units) {
			pred.predictedPos = pred.actualPos;
			pred.basePos = pred.lastRenderedPos;
		}
	}

	if (prevUnit.actualPos == info.pos) {
		if (_update) {
			prevUnit.predictedPos = prevUnit.actualPos;
			prevUnit.basePos = prevUnit.lastRenderedPos;
		}
	}
	else {
		auto predOffset = info.pos - prevUnit.actualPos;
		prevUnit.actualPos = info.pos;
		if (std::abs(predOffset.x) >= 2 << 16 || std::abs(predOffset.y) >= 2 << 16) {
			prevUnit.basePos = info.pos;
			prevUnit.predictedPos = info.pos;
		}
		else {
			prevUnit.basePos = prevUnit.lastRenderedPos;
			prevUnit.predictedPos = info.pos + predOffset;
		}
	}

	float predictionTime = fixedToFloat(D2_FRAME_LENGTH + _frameTimeAdjustment);
	float currentTime = fixedToFloat(_sinceLastUpdate + _frameTimeAdjustment);
	float predFract = currentTime / predictionTime;

	auto offsetFromActual = fixedToFloat(prevUnit.predictedPos - prevUnit.actualPos) * predFract;
	auto offsetFromBase = fixedToFloat(prevUnit.predictedPos - prevUnit.basePos) * predFract;
	
	OffsetF renderOffset;
	if (offsetFromActual.x == 0.f && offsetFromActual.y == 0.f) {
		renderOffset = offsetFromBase;
	}
	else {
		//renderOffset = offsetFromBase;
		renderOffset = offsetFromBase* (1.f - predFract) + offsetFromActual * predFract;
	}

	auto renderPos = fixedToFloat(prevUnit.basePos) + renderOffset;
	auto offset = renderPos - fixedToFloat(prevUnit.actualPos);
	prevUnit.lastRenderedPos = floatToFixed(renderPos);

	const OffsetF scaleFactors{ 32.0f / std::sqrt(2.0f), 16.0f / std::sqrt(2.0f) };
	OffsetF screenOffset = scaleFactors * OffsetF{ offset.x - offset.y, offset.x + offset.y } + 0.5f;
	prevUnit.lastRenderedScreenOffset = { (int32_t)screenOffset.x, (int32_t)screenOffset.y };
	
	_units.push_back(prevUnit);
	return prevUnit.lastRenderedScreenOffset;
}

_Use_decl_annotations_
Offset UnitMotionPredictor::GetShadowOffset(
	Offset screenPos)
{
	for (auto &unit: _prevUnits) {
		if (std::abs(unit.screenPos.x - screenPos.x) < 10 && std::abs(unit.screenPos.y - screenPos.y) < 10) {
			return unit.lastRenderedScreenOffset;
		}
	}
	return { 0, 0 };
}


_Use_decl_annotations_
void UnitMotionPredictor::StartShadow(
	Offset screenPos,
	std::size_t vertexStart)
{
	_shadows.push_back(Shadow(screenPos, vertexStart));
}

_Use_decl_annotations_
void UnitMotionPredictor::AddShadowVerticies(
	std::size_t vertexEnd)
{
	_shadows.back().vertexEnd = vertexEnd;
}

_Use_decl_annotations_
void UnitMotionPredictor::UpdateShadowVerticies(
	Vertex *vertices)
{
	for (auto& shadow : _shadows) {
		for (auto unit = _units.rbegin(), end = _units.rend(); unit != end; ++unit) {
			if (std::abs(unit->screenPos.x - shadow.screenPos.x) < 10 && std::abs(unit->screenPos.y - shadow.screenPos.y) < 10) {
				for (auto i = shadow.vertexStart; i != shadow.vertexEnd; ++i) {
					vertices[i].AddOffset(unit->lastRenderedScreenOffset.x, unit->lastRenderedScreenOffset.y);
				}
			}
		}
	}
}

_Use_decl_annotations_
void UnitMotionPredictor::PrepareForNextFrame(
	int32_t timeToNext)
{
	std::swap(_units, _prevUnits);
	std::stable_sort(_prevUnits.begin(), _prevUnits.end(), [&](auto& x, auto& y) { return x.unit < y.unit; });
	_units.clear();
	_shadows.clear();
	_update = false;

	auto timeBeforeUpdate = D2_FRAME_LENGTH - _sinceLastUpdate;
	_sinceLastUpdate += timeToNext;
	if (_sinceLastUpdate >= D2_FRAME_LENGTH - 10) {
		_update = true;
		_sinceLastUpdate -= D2_FRAME_LENGTH;
		_frameTimeAdjustment = timeBeforeUpdate;
		if (_sinceLastUpdate >= D2_FRAME_LENGTH) {
			_prevUnits.clear();
			_sinceLastUpdate = 0;
			_frameTimeAdjustment = 0;
		}
	}
}