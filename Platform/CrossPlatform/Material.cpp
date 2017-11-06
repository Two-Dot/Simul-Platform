#include "Simul/Platform/CrossPlatform/Material.h"
#include "Simul/Platform/CrossPlatform/Effect.h"

using namespace simul;
using namespace crossplatform;

Material::Material(const char *n) : effect(nullptr),name(n)
{

}

Material::~Material()
{
	InvalidateDeviceObjects();
} 

void Material::InvalidateDeviceObjects()
{
	effect= nullptr;
}

void Material::SetEffect(crossplatform::Effect *e)
{
	effect=e;
}
crossplatform::Effect *Material::GetEffect()
{
	return effect;
}