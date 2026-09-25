/** @file Interface of the library-side support for the headless server. */
#pragma once

#include <string>

#include "player.h"

namespace devilution {

/**
 * @brief Difficulty for games created by the headless dedicated server.
 *
 * multi.cpp reads this (when HeadlessMode is set) instead of the value the
 * game-selection dialog would normally provide.
 */
extern int HeadlessServerDifficulty;

/**
 * @brief Address or game name to join instead of hosting.
 *
 * Empty (the default) means the headless process hosts a game. When set,
 * InitMulti() joins the given game instead: a TCP/IP host address for the
 * TCP provider, or the advertised game name for ZeroTier.
 */
extern std::string HeadlessServerJoinAddress;

/**
 * @brief Configure the idle server player for HeadlessServerSelectHero().
 *
 * Called by the server main from its --hero-class / --hero-name options
 * before the network initialization triggers the hero selection.
 */
void HeadlessServerSelectHeroClass(HeroClass heroClass, const char *heroName);

/**
 * @brief Headless replacement for the hero selection dialog.
 *
 * Reuses the existing save of the configured hero class if one exists,
 * otherwise creates a fresh save; sets gSaveNumber accordingly.
 * Called from SNetInitializeProvider() when HeadlessMode is set.
 */
bool HeadlessServerSelectHero();

} // namespace devilution
