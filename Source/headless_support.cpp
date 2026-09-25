/**
 * @file headless_support.cpp
 *
 * Library-side support for the headless dedicated server
 * (Source/headless_server.cpp).
 *
 * These symbols are referenced from the multiplayer engine code (multi.cpp,
 * storm_net.cpp) whenever HeadlessMode is set, so they must be part of
 * libdevilutionx rather than of the server executable.
 */

#include "headless_support.hpp"

#include <cstdint>
#include <string>

#include "DiabloUI/diabloui.h"
#include "menu.h"
#include "pfile.h"
#include "player.h"
#include "utils/language.h"
#include "utils/log.hpp"
#include "utils/utf8.hpp"

namespace devilution {

int HeadlessServerDifficulty = DIFF_NORMAL;
std::string HeadlessServerJoinAddress;

/**
 * @brief Headless replacement for the hero selection dialog.
 *
 * Reuses the existing save of the configured hero class if one exists
 * (e.g. from a previous server run), otherwise creates a fresh save.
 * Sets gSaveNumber, which InitMulti() later loads via
 * pfile_read_player_from_save().
 *
 * The wanted class is communicated through HeadlessServerSelectHeroClass(),
 * which the server main sets from its --hero-class / --hero-name options
 * before the network initialization starts the hero selection.
 */
namespace {

HeroClass WantedHeroClass = HeroClass::Warrior;
const char *WantedHeroName = "Server";
uint32_t ReuseSaveNumber = UINT32_MAX;

} // namespace

void HeadlessServerSelectHeroClass(HeroClass heroClass, const char *heroName)
{
	WantedHeroClass = heroClass;
	WantedHeroName = heroName;
}

bool HeadlessServerSelectHero()
{
	gbIsMultiplayer = true;

	ReuseSaveNumber = UINT32_MAX;
	pfile_ui_set_hero_infos([](_uiheroinfo *pInfo) {
		if (pInfo->heroclass == WantedHeroClass && ReuseSaveNumber == UINT32_MAX)
			ReuseSaveNumber = pInfo->saveNumber;
		return true;
	});

	if (ReuseSaveNumber != UINT32_MAX) {
		gSaveNumber = ReuseSaveNumber;
		Log("Reusing existing hero save slot {}", gSaveNumber);
		return true;
	}

	_uiheroinfo heroInfo {};
	heroInfo.saveNumber = pfile_ui_get_first_unused_save_num();
	CopyUtf8(heroInfo.name, WantedHeroName, sizeof(heroInfo.name));
	heroInfo.heroclass = WantedHeroClass;
	if (!pfile_ui_save_create(&heroInfo)) {
		LogError("Unable to create hero save");
		return false;
	}
	gSaveNumber = heroInfo.saveNumber;
	Log("Created hero '{}' (save slot {}) for the idle server player", WantedHeroName, gSaveNumber);
	return true;
}

} // namespace devilution
