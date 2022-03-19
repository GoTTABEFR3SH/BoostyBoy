#pragma once

#include "bakkesmod/plugin/bakkesmodplugin.h"
#include "bakkesmod/plugin/pluginwindow.h"
#include "bakkesmod/plugin/PluginSettingsWindow.h"
#include "bakkesmod/wrappers/GameObject/Stats/StatEventWrapper.h"
#include <deque>
#include <stdlib.h>
#include "version.h"

constexpr auto plugin_version = stringify(VERSION_MAJOR) "." stringify(VERSION_MINOR) "." stringify(VERSION_PATCH) "." stringify(VERSION_BUILD);


class BoostyBoy: public BakkesMod::Plugin::BakkesModPlugin, public BakkesMod::Plugin::PluginSettingsWindow // public BakkesMod::Plugin::PluginWindow*/
{

	
	virtual void onLoad(); 
	virtual void onUnload();
	
	struct stats {
		int smallPads;
		int bigPads;
		double totalBoostUsed;
		char winOrLoss;

		friend std::istream& operator>>(std::istream& is, stats& s) {
			is >> s.totalBoostUsed;
			is >> s.bigPads;
			is >> s.smallPads;
			is >> s.winOrLoss;
			return is;
		}

		friend std::ostream& operator<<(std::ostream& os, stats& s) {
			os << s.totalBoostUsed << std::endl;
			os << s.bigPads << std::endl;
			os << s.smallPads << std::endl;
			os << s.winOrLoss;

			return os;
		}
	};
	struct StatEventParams {
		// always primary player
		uintptr_t PRI;
		// wrapper for the stat event
		uintptr_t StatEvent;
	};
	stats singleGameStats;
	stats averageStats;
	stats averageWinStats;
	stats averageLossStats;
	
	void ballOnTop();
	void Render(CanvasWrapper canvas);

	float getCurrentBoostAmount();
	
	stats getAverage(char result,std::deque<stats>tempQueue);
	void addToTotal(double boostUsed);
	void addToAverage(stats tempStatStruct, std::deque<stats>&tempQForStatValues); 
	void collectPad(BoostPickupWrapper bpw);
	void gameStart();
	void setStatsToZero();
	stats getAverage(std::deque<stats>&tempQueue);
	void newBoostToggle();
	void newCalculateBoostUsed();
	void getBoostUsedOnline();
	void readFromTextFile(std::string textFile, std::deque<stats>&tempQueue);
	void saveToTextFile(std::string textFile, std::deque<stats>&tempQueue);
	void onStatEvent(void* params);
	
	std::deque<stats> statsFromPreviousMatches;
	


	void RenderSettings() override;
	std::string GetPluginName() override;
	void SetImGuiContext(uintptr_t ctx) override;
	void overlaySettings();
	void statSettings();
	void displayStatTable(std::deque<stats> tempQueue);
	bool coolEnabled;
	bool renderEnabled;
	bool showBoostStats;
	bool showPadStats;
	bool hasGameBeenOpened = false;
	std::chrono::time_point<std::chrono::system_clock> start, end;
	
	
	bool safeGuardSwitch = false;
	bool boostSwitchForOnlineGames = true;
	float currentBoostAmount;
	double boostUsed;
	double totalBoostUsedInAMatch;
	int amountOfGamesToTrack = 5;
	int smallBoostPads;
	int bigBoostPads;
	int wins, losses;
	std::string textfile = "BoostyBoyStats.txt";
	std::string timeSpentBoosting;
};

