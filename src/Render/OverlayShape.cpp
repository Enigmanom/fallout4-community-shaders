#include "Render/OverlayShape.h"

#include "RE/B/BSGeometry.h"
#include "RE/B/BSRenderPass.h"
#include "RE/B/BSShaderProperty.h"
#include "RE/N/NiAlphaProperty.h"

namespace cs::engine
{
	bool IsOverlayShape(RE::BSRenderPass* a_pass) noexcept
	{
		constexpr std::uint16_t kAlphaTestFlag = 0x200;
		const auto* geometry = a_pass->GetGeometry();
		const auto* property = a_pass->GetShaderProperty();
		if (!geometry || !property)
			return false;
		const auto* alphaProperty = netimmerse_cast<RE::NiAlphaProperty*>(geometry->properties[0].get());
		if (alphaProperty && (alphaProperty->flags.flags & kAlphaTestFlag) != 0)
			return true;
		using enum RE::BSShaderProperty::EShaderPropertyFlag;
		return property->flags.any(kDecal, kDynamicDecal);
	}
}
