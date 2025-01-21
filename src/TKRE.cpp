#include "TKRE.h"
#include <string>
#include <DKUtil/Logger.hpp>

#define SETTINGFILE_PATH "Data\\SKSE\\Plugins\\TK Dodge RE.ini"

#define PI 3.14159265f
#define PI8 0.39269908f

inline RE::NiPoint2 Vec2Rotate(const RE::NiPoint2& vec, float angle)
{
	RE::NiPoint2 ret;
	ret.x = vec.x * cos(angle) - vec.y * sin(angle);
	ret.y = vec.x * sin(angle) + vec.y * cos(angle);
	return ret;
}

inline float Vec2Length(const RE::NiPoint2& vec)
{
	return std::sqrtf(vec.x * vec.x + vec.y * vec.y);
}

inline RE::NiPoint2 Vec2Normalize(RE::NiPoint2& vec)
{
	RE::NiPoint2 ret(0.f, 0.f);
	float vecLength = Vec2Length(vec);
	if (vecLength == 0) {
		return ret;
	}
	float invlen = 1.0f / vecLength;
	ret.x = vec.x * invlen;
	ret.y = vec.y * invlen;
	return ret;
}

inline float DotProduct(RE::NiPoint2& a, RE::NiPoint2& b)
{
	return a.x * b.x + a.y * b.y;
}

inline float CrossProduct(RE::NiPoint2& a, RE::NiPoint2& b)
{
	return a.x * b.y - a.y * b.x;
}

inline float GetAngle(RE::NiPoint2& a, RE::NiPoint2& b)
{
	return atan2(CrossProduct(a, b), DotProduct(a, b));
}

inline bool isJumping(RE::Actor* a_actor)
{
	bool result = false;
	return a_actor->GetGraphVariableBool("bInJumpState", result) && result;
}

bool TKRE::GetDodgeEvent(std::string& a_event)
{
	auto normalizedInputDirection = Vec2Normalize(RE::PlayerControls::GetSingleton()->data.prevMoveVec);
	if (normalizedInputDirection.x == 0.f && normalizedInputDirection.y == 0.f) {
		return false;
	}

	auto tdm_freeMov = RE::TESForm::LookupByEditorID<RE::TESGlobal>("TDM_DirectionalMovement");
	if (tdm_freeMov && tdm_freeMov->value) {
		DEBUG("TDM Free Movement, Force to Forward Dodge!");
		a_event = "TKDodgeForward";
	} else {
		RE::NiPoint2 forwardVector(0.f, 1.f);
		float dodgeAngle = GetAngle(normalizedInputDirection, forwardVector);
		if (dodgeAngle >= -2 * PI8 && dodgeAngle < 2 * PI8) {
			a_event = "TKDodgeForward";
		} else if (dodgeAngle >= -6 * PI8 && dodgeAngle < -2 * PI8) {
			a_event = "TKDodgeLeft";
		} else if (dodgeAngle >= 6 * PI8 || dodgeAngle < -6 * PI8) {
			a_event = "TKDodgeBack";
		} else if (dodgeAngle >= 2 * PI8 && dodgeAngle < 6 * PI8) {
			a_event = "TKDodgeRight";
		}
	}

	return true;
}

//bunch of ugly checks
inline bool canDodge(RE::PlayerCharacter* a_pc)
{
	auto playerControls = RE::PlayerControls::GetSingleton();
	bool bIsDodging = false;
	auto controlMap = RE::ControlMap::GetSingleton();
	const auto playerState = a_pc->AsActorState();
	auto attackState = playerState->GetAttackState();

	float StaminaCost = Settings::dodgeStamina;
	if (Settings::WeightFactor != 0.f)
	{ 
		StaminaCost = Settings::dodgeStamina + Settings::WeightFactor * a_pc->GetEquippedWeight();
	}
	

	return a_pc->GetGraphVariableBool("bIsDodging", bIsDodging) && !bIsDodging && 
		((attackState == RE::ATTACK_STATE_ENUM::kNone) || Settings::enableDodgeAttackCancel) && 
		(!playerState->IsSprinting() || !Settings::EnableSprintKeyDodge) && 
		(controlMap->IsMovementControlsEnabled() && controlMap->IsFightingControlsEnabled()) &&
	        (!playerState->IsSneaking() || Settings::enableSneakDodge) && playerControls && 
		playerControls->attackBlockHandler && playerControls->attackBlockHandler->inputEventHandlingEnabled && playerControls->movementHandler &&
	        playerControls->movementHandler->inputEventHandlingEnabled && 
		(playerState->GetSitSleepState() == RE::SIT_SLEEP_STATE::kNormal && playerState->GetKnockState() == RE::KNOCK_STATE_ENUM::kNormal && playerState->GetFlyState() == RE::FLY_STATE::kNone) && 
		!playerState->IsSwimming() && !isJumping(a_pc) && !a_pc->IsInKillMove() && 
		(a_pc->AsActorValueOwner()->GetActorValue(RE::ActorValue::kStamina) >= (Settings::EnableLowStamina ? Settings::MinStamina : StaminaCost));
}

void TKRE::dodge()
{
	DEBUG("dodging");
	auto pc = RE::PlayerCharacter::GetSingleton();
	if (!canDodge(pc)) {
		DEBUG("cannot dodge");
		return;
	}
	std::string dodge_event = Settings::defaultDodgeEvent;
	if (!GetDodgeEvent(dodge_event) && !Settings::EnableDodgeInPlace)
		return;

	if (Settings::stepDodge) {
		pc->SetGraphVariableInt("iStep", 2);
	}
	pc->SetGraphVariableFloat("TKDR_IframeDuration", Settings::iFrameDuration);  //Set invulnerable frame duration

	pc->NotifyAnimationGraph(dodge_event);                                       //Send TK Dodge Event

}

void TKRE::applyDodgeCost()
{
	auto pc = RE::PlayerCharacter::GetSingleton();
	if (pc && !pc->IsGodMode()) 
	{
		float StaminaCost = Settings::dodgeStamina;
		if (Settings::WeightFactor != 0.f)
		{ 
			StaminaCost = Settings::dodgeStamina + Settings::WeightFactor * pc->GetEquippedWeight();
		}
		float CurrentStamina = pc->AsActorValueOwner()->GetActorValue(RE::ActorValue::kStamina);
		pc->AsActorValueOwner()->RestoreActorValue(RE::ACTOR_VALUE_MODIFIER::kDamage, RE::ActorValue::kStamina,
		 -((Settings::EnableLowStamina && CurrentStamina < StaminaCost) ? CurrentStamina : StaminaCost)); // Not sure how the game handles negative stamina 
	}

}

void Settings::ReadIntSetting(CSimpleIniA& a_ini, const char* a_sectionName, const char* a_settingName, uint32_t& a_setting)
{
	const char* bFound = nullptr;
	bFound = a_ini.GetValue(a_sectionName, a_settingName);
	if (bFound) {
		INFO("found {} with value {}", a_settingName, bFound);
		a_setting = static_cast<int>(a_ini.GetDoubleValue(a_sectionName, a_settingName));
	}
}
void Settings::ReadFloatSetting(CSimpleIniA& a_ini, const char* a_sectionName, const char* a_settingName, float& a_setting)
{
	const char* bFound = nullptr;
	bFound = a_ini.GetValue(a_sectionName, a_settingName);
	if (bFound) {
		INFO("found {} with value {}", a_settingName, bFound);
		a_setting = static_cast<float>(a_ini.GetDoubleValue(a_sectionName, a_settingName));
	}
}

void Settings::ReadBoolSetting(CSimpleIniA& a_ini, const char* a_sectionName, const char* a_settingName, bool& a_setting)
{
	const char* bFound = nullptr;
	bFound = a_ini.GetValue(a_sectionName, a_settingName);
	if (bFound) {
		INFO("found {} with value {}", a_settingName, bFound);
		a_setting = a_ini.GetBoolValue(a_sectionName, a_settingName);
	}
}

void Settings::readSettings()
{
	INFO("Reading Settings...");
	CSimpleIniA ini;
	ini.LoadFile(SETTINGFILE_PATH);

	ReadIntSetting(ini, "Main", "DodgeHotkey", dodgeKey);
	ReadBoolSetting(ini, "Main", "EnableSprintKeyDodge", EnableSprintKeyDodge);
	ReadBoolSetting(ini, "Main", "EnableSneakKeyDodge", EnableSneakKeyDodge);
	ReadBoolSetting(ini, "Main", "EnableDodgeInPlace", EnableDodgeInPlace);
	ReadBoolSetting(ini, "Main", "StepDodge", stepDodge);
	ReadBoolSetting(ini, "Main", "enableDodgeAttackCancel", enableDodgeAttackCancel);
	ReadFloatSetting(ini, "Main", "DodgeStamina", dodgeStamina);
	ReadBoolSetting(ini, "Main", "EnableSneakDodge", enableSneakDodge);
	ReadFloatSetting(ini, "Main", "iFrameDuration", iFrameDuration);
	defaultDodgeEvent = ini.GetValue("Main", "defaultDodgeEvent", "TKDodgeBack");
	ReadFloatSetting(ini, "Main", "SprintingPressDuration", SprintingPressDuration);
	if (iFrameDuration < 0.f) {
		iFrameDuration = 0.f;
	}

	if (ini.GetBoolValue("Main", "EnableDebugLog")) {
		spdlog::set_level(spdlog::level::debug);
		DEBUG("Debug log enabled");
	}
	ReadFloatSetting(ini, "Main", "WeightFactor", WeightFactor);
	ReadBoolSetting(ini, "Main", "EnableLowStamina", EnableLowStamina);
	ReadFloatSetting(ini, "Main", "MinStamina", MinStamina);

// Not sure how the game handles negative stamina
	if (MinStamina < 0.f)
	{
		MinStamina = 0.f;
	}
	INFO("Step dodge: %d", stepDodge);
	INFO("...done");
}
