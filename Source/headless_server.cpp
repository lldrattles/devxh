/**
 * @file headless_server.cpp
 *
 * Implementation of a headless (no display / no window manager) dedicated
 * server for DevilutionX.
 *
 * The server runs the real game engine with a real "idle" player so that it
 * can act as the lock-step simulation host, but it never opens a window and
 * never renders anything: SDL video/audio/cursor subsystems are skipped by
 * leveraging the existing HeadlessMode code paths, and all user interaction
 * normally performed by the menus is handled automatically here.
 *
 * It hosts a public game (visible in the game list to anyone) over TCP/IP
 * (optionally bound to a specific address, e.g. a ZeroTier interface) or over
 * the built-in ZeroTier provider, and idles forever while clients come and go.
 */

#include <cstdint>
#include <cstdio>
#include <csignal>
#include <cstring>
#include <string>

#include <SDL.h>

#ifdef USE_SDL1
#include "utils/sdl2_to_1_2_backports.h"
#else
#include "utils/sdl2_backports.h"
#endif

#include "control.h"
#include "diablo.h"
#include "headless_support.hpp"
#include "DiabloUI/diabloui.h"
#include "engine/demomode.h"
#include "engine/sound.h"
#include "init.h"
#include "menu.h"
#include "multi.h"
#include "nthread.h"
#include "options.h"
#include "pfile.h"
#include "player.h"
#include "playerdat.hpp"
#include "storm/storm_net.hpp"
#include "utils/display.h"
#include "utils/file_util.h"
#include "utils/language.h"
#include "utils/console.h"
#include "utils/log.hpp"
#include "utils/paths.h"
#include "utils/utf8.hpp"

#ifdef GPERF_HEAP_MAIN
#include <gperftools/heap-profiler.h>
#endif

namespace devilution {

bool HeadlessServerMain(int argc, char **argv);

// Defined in DiabloUI/multi/selconn.cpp (normally selected in the connection dialog).
extern int provider;

namespace {

struct ServerConfig {
	uint32_t provider = SELCONN_TCP;
	std::string bindAddress = "0.0.0.0";
	std::string gameName;
	std::string password;
	std::string heroName = "Server";
	HeroClass heroClass = HeroClass::Warrior;
	std::string game = "hellfire";
	int difficulty = DIFF_NORMAL;
	bool fullQuests = false;
	// Empty means host a game; otherwise join the given game (TCP: host
	// address, ZeroTier: game name) as an idle player.
	std::string joinAddress;
};

ServerConfig cfg;

void PrintServerHelpAndExit()
{
	printInConsole("Usage: devilutionx-server [options]\n");
	printNewlineInConsole();
	printInConsole("Headless dedicated server. Hosts a public multiplayer game and idles.\n");
	printNewlineInConsole();
	printInConsole("  --server-tcp [ADDRESS]     Host a TCP/IP game (default). Binds to all interfaces\n");
	printInConsole("                             by default; give an address to bind to, e.g. a\n");
	printInConsole("                             ZeroTier interface address.\n");
	printInConsole("  --server-zerotier          Host a ZeroTier game on the configured network.\n");
	printInConsole("  --server-zt-network ID     Join the ZeroTier network with the given 16-hex-digit\n");
	printInConsole("                             ID before hosting (default: Diasurgical public Earth).\n");
	printInConsole("  --join ADDRESS             Do not host: join the game at ADDRESS as an idle\n");
	printInConsole("                             player instead (TCP/IP host name or IP, or the\n");
	printInConsole("                             advertised game name for ZeroTier).\n");
	printInConsole("  --name NAME                Game name shown in the game list (default: server-<pid>).\n");
	printInConsole("  --password PASSWORD        Password of the game (hosted or joined; default:\n");
	printInConsole("                             public game).\n");
	printInConsole("  --port PORT                TCP port to listen on (default: 6112).\n");
	printInConsole("  --hero-class CLASS         Idle player class: warrior rogue sorcerer monk bard\n");
	printInConsole("                             barbarian (default: warrior).\n");
	printInConsole("  --hero-name NAME           Idle player name (default: Server).\n");
	printInConsole("  --game GAME                hellfire or diablo (default: hellfire). Must match\n");
	printInConsole("                             the MPQ data available to the server.\n");
	printInConsole("  --difficulty N             0 = normal, 1 = nightmare, 2 = hell (default: 0).\n");
	printInConsole("                             Only applies when hosting.\n");
	printInConsole("  --full-quests              Enable the full quest set for the game.\n");
	printInConsole("  --data-dir PATH            Asset (MPQ) search directory.\n");
	printInConsole("  --save-dir PATH            Directory for the server hero save.\n");
	printInConsole("  --config-dir PATH          Directory for configuration files.\n");
	printInConsole("  -f                         Display frame count in the log.\n");
	printInConsole("  --verbose                  More verbose logging.\n");
	printInConsole("  -h, --help                 Show this help.\n");
	printNewlineInConsole();
	printInConsole("Standard clients join the game the same way they join any other game:\n");
	printInConsole("TCP/IP: enter the server address. ZeroTier: pick the game from the list.\n");
	diablo_quit(0);
}

HeroClass ParseHeroClass(string_view name)
{
	if (name == "warrior")
		return HeroClass::Warrior;
	if (name == "rogue")
		return HeroClass::Rogue;
	if (name == "sorcerer")
		return HeroClass::Sorcerer;
	if (name == "monk")
		return HeroClass::Monk;
	if (name == "bard")
		return HeroClass::Bard;
	if (name == "barbarian")
		return HeroClass::Barbarian;
	printInConsole("unrecognized hero class '");
	printInConsole(name.data());
	printInConsole("'");
	printNewlineInConsole();
	PrintServerHelpAndExit();
	return HeroClass::Warrior;
}

void ParseServerArgs(int argc, char **argv)
{
	for (int i = 1; i < argc; i++) {
		const string_view arg = argv[i];
		if (arg == "-h" || arg == "--help") {
			PrintServerHelpAndExit();
		} else if (arg == "--version") {
			printInConsole(PROJECT_NAME);
			printInConsole(" v");
			printInConsole(PROJECT_VERSION);
			printNewlineInConsole();
			diablo_quit(0);
		} else if (arg == "--server-tcp") {
			cfg.provider = SELCONN_TCP;
			if (i + 1 < argc && argv[i + 1][0] != '-')
				cfg.bindAddress = argv[++i];
		} else if (arg == "--server-zerotier") {
			cfg.provider = SELCONN_ZT;
		} else if (arg == "--join") {
		if (i + 1 == argc) {
			printInConsole("--join requires an argument");
			printNewlineInConsole();
			diablo_quit(64);
		}
		cfg.joinAddress = argv[++i];
	} else if (arg == "--server-zt-network") {
			if (i + 1 == argc) {
				printInConsole("--server-zt-network requires an argument");
				printNewlineInConsole();
				diablo_quit(64);
			}
			HeadlessZTNetwork = argv[++i];
		} else if (arg == "--name") {
			if (i + 1 == argc) {
				printInConsole("--name requires an argument");
				printNewlineInConsole();
				diablo_quit(64);
			}
			cfg.gameName = argv[++i];
		} else if (arg == "--password") {
			if (i + 1 == argc) {
				printInConsole("--password requires an argument");
				printNewlineInConsole();
				diablo_quit(64);
			}
			cfg.password = argv[++i];
		} else if (arg == "--port") {
			if (i + 1 == argc) {
				printInConsole("--port requires an argument");
				printNewlineInConsole();
				diablo_quit(64);
			}
			sgOptions.Network.port.SetValue(SDL_atoi(argv[++i]));
		} else if (arg == "--hero-class") {
			if (i + 1 == argc) {
				printInConsole("--hero-class requires an argument");
				printNewlineInConsole();
				diablo_quit(64);
			}
			cfg.heroClass = ParseHeroClass(argv[++i]);
		} else if (arg == "--game") {
			if (i + 1 == argc) {
				printInConsole("--game requires an argument");
				printNewlineInConsole();
				diablo_quit(64);
			}
			cfg.game = argv[++i];
			if (cfg.game != "hellfire" && cfg.game != "diablo") {
				printInConsole("--game must be 'hellfire' or 'diablo'");
				printNewlineInConsole();
				diablo_quit(64);
			}
		} else if (arg == "--hero-name") {
			if (i + 1 == argc) {
				printInConsole("--hero-name requires an argument");
				printNewlineInConsole();
				diablo_quit(64);
			}
			cfg.heroName = argv[++i];
		} else if (arg == "--difficulty") {
			if (i + 1 == argc) {
				printInConsole("--difficulty requires an argument");
				printNewlineInConsole();
				diablo_quit(64);
			}
			cfg.difficulty = SDL_atoi(argv[++i]);
			if (cfg.difficulty < DIFF_NORMAL || cfg.difficulty > DIFF_HELL)
				cfg.difficulty = DIFF_NORMAL;
		} else if (arg == "--full-quests") {
			cfg.fullQuests = true;
		} else if (arg == "--data-dir") {
			if (i + 1 == argc) {
				printInConsole("--data-dir requires an argument");
				printNewlineInConsole();
				diablo_quit(64);
			}
			paths::SetBasePath(argv[++i]);
		} else if (arg == "--save-dir") {
			if (i + 1 == argc) {
				printInConsole("--save-dir requires an argument");
				printNewlineInConsole();
				diablo_quit(64);
			}
			paths::SetPrefPath(argv[++i]);
		} else if (arg == "--config-dir") {
			if (i + 1 == argc) {
				printInConsole("--config-dir requires an argument");
				printNewlineInConsole();
				diablo_quit(64);
			}
			paths::SetConfigPath(argv[++i]);
		} else if (arg == "-n") {
			// The splash screen is always skipped on a server; accepted for compatibility.
		} else if (arg == "-f") {
			EnableFrameCount();
		} else if (arg == "--verbose") {
			SDL_LogSetAllPriority(SDL_LOG_PRIORITY_VERBOSE);
		} else {
			printInConsole("unrecognized option '");
			printInConsole(argv[i]);
			printInConsole("'");
			printNewlineInConsole();
			PrintServerHelpAndExit();
		}
	}
}

void ServerInitHeadless()
{
	HeadlessMode = true;
	gbShowIntro = false;
	gbMusicOn = false;
	gbSoundOn = false;
	AdjustToScreenGeometry(forceResolution);
}

/**
 * @brief Translate SIGINT/SIGTERM into SDL_QUIT events.
 *
 * SDL only installs its signal-to-SDL_QUIT handler when the video subsystem
 * is initialized, which the headless server deliberately skips. Without this
 * the server could only be stopped with SIGKILL.
 */
void HandleServerSignal(int)
{
	SDL_Event quitEvent {};
	quitEvent.type = SDL_QUIT;
	SDL_PushEvent(&quitEvent);
}

void InstallServerSignalHandlers()
{
	struct sigaction action {};
	action.sa_handler = HandleServerSignal;
	sigaction(SIGINT, &action, nullptr);
	sigaction(SIGTERM, &action, nullptr);
}	void ServerInitNetwork()
{
#ifdef DISABLE_ZERO_TIER
	if (cfg.provider == SELCONN_ZT) {
		app_fatal("This binary was built without ZeroTier support.");
	}
#endif
#ifdef DISABLE_TCP
	if (cfg.provider == SELCONN_TCP) {
		app_fatal("This binary was built without TCP support.");
	}
#endif
	if (cfg.provider == SELCONN_TCP) {
		CopyUtf8(sgOptions.Network.szBindAddress, cfg.bindAddress,
		    sizeof(sgOptions.Network.szBindAddress));
	}
}

} // namespace

bool HeadlessServerMain(int argc, char **argv)
{
#ifdef _DEBUG
	SDL_LogSetAllPriority(SDL_LOG_PRIORITY_DEBUG);
#endif

	ParseServerArgs(argc, argv);
	InitKeymapActions();
	InitPadmapActions();

	// Fix the game mode so that DiabloInit() never presents the interactive
	// "play Diablo or Hellfire?" startup dialog (there is nobody to answer it).
	if (cfg.game == "hellfire")
		forceHellfire = true;
	else
		forceDiablo = true;

	if (cfg.gameName.empty())
		cfg.gameName = fmt::format("server-{}", getpid());

	// The stock client lets SDL create the pref directory; a custom
	// --save-dir / --config-dir must be created explicitly for the server.
	RecursivelyCreateDir(paths::PrefPath().c_str());
	RecursivelyCreateDir(paths::ConfigPath().c_str());

	// Need to ensure devilutionx.mpq (and fonts.mpq if available) are loaded
	// before attempting to read translation settings.
	LoadCoreArchives();
	was_archives_init = true;

	// Read settings including translation next. This will use the presence of
	// fonts.mpq and look for assets in devilutionx.mpq.
	LoadOptions();
	// Then look for a voice pack file based on the selected translation.
	LoadLanguageArchive();

	if (cfg.fullQuests)
		sgOptions.Gameplay.multiplayerFullQuests.SetValue(true);

	ServerInitHeadless();
	ServerInitNetwork();

	ApplicationInit();
	SaveOptions();

	// LoadGameArchives() detects Hellfire; the server always runs Hellfire
	// when its data is present, matching the client behavior.
	LoadGameArchives();
	if (!gbIsHellfire)
		Log("Hellfire data not found, hosting a Diablo game instead.");

	DiabloInit();
	SaveOptions();

	// For TCP, the "game name" given to the network layer doubles as the
	// bind address (this is how the stock client behaves as well); for
	// ZeroTier it is the name advertised in the game list.
	gbIsMultiplayer = true;
	GameName = (cfg.provider == SELCONN_TCP) ? cfg.bindAddress : cfg.gameName;
	GamePassword = cfg.password;
	provider = static_cast<int>(cfg.provider);
	HeadlessServerJoinAddress = cfg.joinAddress;
	if (cfg.joinAddress.empty())
		HeadlessServerDifficulty = cfg.difficulty;
	HeadlessServerSelectHeroClass(cfg.heroClass, cfg.heroName.c_str());

	if (cfg.joinAddress.empty()) {
		Log("Hosting game '{}' on {} (idle player: {})", cfg.gameName,
		    cfg.provider == SELCONN_TCP ? "TCP/IP" : "ZeroTier", cfg.heroName);
	} else {
		Log("Joining game at '{}' on {} as an idle player ({})", cfg.joinAddress,
		    cfg.provider == SELCONN_TCP ? "TCP/IP" : "ZeroTier", cfg.heroName);
	}

	// StartGame hosts the game and runs the simulation until it ends.
	InstallServerSignalHandlers();
	StartGame(true, false);

	DiabloDeinit();

	return 0;
}

} // namespace devilution

extern "C" int main(int argc, char **argv)
{
#ifdef GPERF_HEAP_MAIN
	HeapProfilerStart("main");
#endif
	const int result = devilution::HeadlessServerMain(argc, argv);
#ifdef GPERF_HEAP_MAIN
	HeapProfilerStop();
#endif
	return result;
}
