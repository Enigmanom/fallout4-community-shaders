#include "Render/LocalLights.h"

#include <algorithm>
#include <cmath>

namespace cs::engine::local_lights
{
	float RadialFalloff(float a_x, float a_constant, float a_scalar, float a_exponent)
	{
		return std::pow(1.0f - std::clamp(a_constant + a_scalar * std::pow(a_x, a_exponent), 0.0f, 1.0f), 2.2f);
	}
	float ShapeAttenuation(RE::BSLight& a_light, const RE::NiPoint3& a_position)
	{
		const auto shape = a_light.GetShape();
		const auto* light = a_light.GetSourceLight();
		if (shape == RE::BSLight::Shape::kBox) {
			const auto delta = a_position - a_light.box.center;
			const auto& rotation = a_light.rotation;
			const auto& extent = a_light.box.halfExtents;
			const auto local = rotation.Transpose() * delta;
			return std::abs(local.x) <= extent.x && std::abs(local.y) <= extent.y &&
			               std::abs(local.z) <= extent.z ?
			           1.0f :
			           0.0f;
		}
		if (shape != RE::BSLight::Shape::kHemisphere && shape != RE::BSLight::Shape::kCone)
			return 1.0f;
		const auto delta = a_position - light->world.translate;
		const float length = std::sqrt(delta.x * delta.x + delta.y * delta.y + delta.z * delta.z);
		const auto& direction = light->world.rotate[0];
		const float cosine = length == 0 ? 0 :
		                                   (delta.x * direction.x + delta.y * direction.y + delta.z * direction.z) / length;
		if (shape == RE::BSLight::Shape::kHemisphere)
			return cosine >= 0 ? 1.0f : 0.0f;
		const float cone = std::clamp(1 - (1 - std::max(cosine, 0.0f)) /
											  (1 - a_light.cone.cosineCutoff),
			0.0f, 1.0f);
		return std::min(std::pow(cone, a_light.cone.exponent), 1.0f);
	}
}
