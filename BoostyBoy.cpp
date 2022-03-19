#include "pch.h"
#include "BoostyBoy.h"
#include <time.h>
#include <chrono>
#include <fstream>
#include <iostream>


BAKKESMOD_PLUGIN(BoostyBoy, "Tracks Boost", plugin_version, PLUGINTYPE_FREEPLAY)

std::shared_ptr<CVarManagerWrapper> _globalCvarManager;

struct EventPickedUpParams
{
	uintptr_t vehicleWrapper;
};


void BoostyBoy::onLoad()           
{
	_globalCvarManager = cvarManager;
	cvarManager->log("BoostyBoy loaded!!");

	/* Varibles for the Overlays position on screen */
	cvarManager->registerCvar("overlayXPosition", "1300", "Set the postion of the overlay on the x axis");
	cvarManager->registerCvar("overlayYPosition", "900", "Set the position of the overlay on the y axis");
	cvarManager->registerCvar("overlayScale", "2", "Sets the scale of the overlay");
	cvarManager->registerCvar("cool_distance", "200.0", "Distance to place the ball above");
	
	cvarManager->registerCvar("amountOfGamesToTrack", "5", "How many games to track", true, true, 5, true, 25, true)
		.addOnValueChanged([this](std::string oldValue, CVarWrapper cvar) {
		amountOfGamesToTrack = cvar.getIntValue();
		averageWinStats = getAverage('W', statsFromPreviousMatches); //Re-Average all stats when the amount of games is changed
		averageLossStats = getAverage('L', statsFromPreviousMatches);
		averageStats = getAverage(statsFromPreviousMatches);

			});

	/* Enable/Disable the drawing of stats on screen */
	cvarManager->registerCvar("render_Enable", "0", "Enables on screen stats", true, false, false, false, false, true)
		.addOnValueChanged([this](std::string oldValue, CVarWrapper cvar) {
		renderEnabled = cvar.getBoolValue();
			});
	cvarManager->registerCvar("boostStats_Enable", "0", "Enables on screen Boost stats", true, false, false, false, false, true)
		.addOnValueChanged([this](std::string oldValue, CVarWrapper cvar) {
		showBoostStats = cvar.getBoolValue();
			});
	cvarManager->registerCvar("padStats_Enable", "0", "Enables on screen Boost Pad stats", true, false, false, false, false, true)
		.addOnValueChanged([this](std::string oldValue, CVarWrapper cvar) {
		showPadStats = cvar.getBoolValue();
			});

	/* Gets The Results of Matches Win/Loss */
	gameWrapper->HookEventWithCallerPost<ServerWrapper>("Function TAGame.GFxHUD_TA.HandleStatEvent",
		[this](ServerWrapper caller, void* params, std::string HandleStatEvent) {
			onStatEvent(params);
		});
		
	/* On Game Start */
	gameWrapper->HookEvent("Function GameEvent_Soccar_TA.WaitingForPlayers.BeginState", [this](std::string BeginState) { 
		gameStart();
		setStatsToZero();  

			});
		
	/* Boost Pad Was Collected*/
		gameWrapper->HookEventWithCallerPost<ActorWrapper>("Function TAGame.VehiclePickup_TA.EventPickedUp", [this](ActorWrapper caller, void* params, std::string eventname) {
			BoostPickupWrapper bpw(caller.memory_address); // Get Our Car
			collectPad(bpw); // Determine if we picked it up, and if so what kind of pad was it.
			});

		/* Game Has Ended */
		gameWrapper->HookEvent("Function TAGame.GameEvent_Soccar_TA.EventMatchEnded", [this](std::string EventMatchEnded) {
		
			
			if (singleGameStats.winOrLoss != 'W') {
				singleGameStats.winOrLoss = 'L';
			}
			addToAverage(singleGameStats, statsFromPreviousMatches); // Add struct to stat queue
			averageWinStats = getAverage('W', statsFromPreviousMatches); // Update all averages
			averageLossStats = getAverage('L', statsFromPreviousMatches);
			averageStats = getAverage(statsFromPreviousMatches);
			saveToTextFile(textfile, statsFromPreviousMatches); // Save the new updated queue to the text file, overwrites the entire file 
			cvarManager->log("Game Stats Saved");
			});
	
		/*Main Menu is up, either by boot or by exiting a game */
		gameWrapper->HookEvent("Function TAGame.GFxData_MainMenu_TA.MainMenuAdded", [this](std::string MainMenuAdded) { // happens when the main menu is opened either by booting or exiting a game mode
			if (hasGameBeenOpened == false){ // We only need this to trigger once on boot, bool keeps from triggering after seeing the main menu again within same play session
				hasGameBeenOpened = true;
				readFromTextFile(textfile, statsFromPreviousMatches); // read in all stats and fill queue, only happens once here
				averageWinStats = getAverage('W', statsFromPreviousMatches); // update averages
				averageLossStats = getAverage('L', statsFromPreviousMatches);
				averageStats = getAverage(statsFromPreviousMatches);
			}
			});
		
		/* Boost has been activated NOTE sometimes this fires in replayes so checks are neccessary */
		gameWrapper->HookEvent("Function TAGame.ProductStat_BoostTime_TA.HandleActivationChanged", [this](std::string HandleActivationChanged) { 
				newBoostToggle();
			});
			
		/* After Goal is scored players are on kickoff*/
		gameWrapper->HookEvent("Function GameEvent_Soccar_TA.Countdown.BeginState", [this](std::string CountDownBeginState) {
			boostSwitchForOnlineGames = true; // sometimes the boostswitch gets flipped during replayes best to reset it afterwards
			});
		
 		/* Called every game tick or 120 times a second */
		gameWrapper->HookEvent("Function TAGame.Car_TA.SetVehicleInput", [this](std::string boostWasUsed) { 
			currentBoostAmount = getCurrentBoostAmount(); //Need this for filtering in BoostPad function and for niche boost usage in the boost toggle function 
			getBoostUsedOnline(); // This is the niche function that gets called when players hold down boost while having 0 then drive over a pad, its pretty dumb I know but it works. 
		
		});
		
		/* Draw Stuff On Screen */
		gameWrapper->RegisterDrawable([this](CanvasWrapper canvas) { 
			Render(canvas);
			});

		}
		
void BoostyBoy::onUnload()
{
	cvarManager->log("BoostyBoy Out!");
}

void BoostyBoy::onStatEvent(void* params)
{
	
	StatEventParams* pStruct = (StatEventParams*)params;
	PriWrapper receiver = PriWrapper(pStruct->PRI);
	StatEventWrapper statEvent = StatEventWrapper(pStruct->StatEvent);

	if (statEvent.GetEventName() == "Win")
	{
		singleGameStats.winOrLoss = 'W';
		LOG("YOU WON THE GAME!");
	}


}



void BoostyBoy::newBoostToggle()
{
	if (!gameWrapper->IsInOnlineGame()) { return; }
	else {

		CarWrapper car = gameWrapper->GetLocalCar(); // Get the car and NULL check it 
		if (!car) { return; }
		BoostWrapper carBoostComponent = car.GetBoostComponent();
		if (!carBoostComponent) { return; }
		if (!carBoostComponent.CanActivate()) { return; }
		
		if (safeGuardSwitch == false)
		{
			safeGuardSwitch = true;
		}
		else if (safeGuardSwitch == true)
		{
			safeGuardSwitch = false;
			if (boostSwitchForOnlineGames == true)
			{
				start = std::chrono::system_clock::now();
				cvarManager->log("Boosting.....");
				boostSwitchForOnlineGames = false;
			}
			else if (boostSwitchForOnlineGames == false)
			{
				newCalculateBoostUsed();
				cvarManager->log("Not Boosting/ Boosting has ended....");
				
				
				
				boostSwitchForOnlineGames = true;
			}
		}
	}
}

void BoostyBoy::newCalculateBoostUsed()
{
	CarWrapper car = gameWrapper->GetLocalCar(); // Get the local car and nullcheck it
	if (!car) { return; }
	BoostWrapper carBoostComponent = car.GetBoostComponent();
	if (!carBoostComponent) { return; }
	float BoostConsumptionRate = carBoostComponent.GetBoostConsumptionRate(); // using the getboostconsumptionrate() function here resulted in a memory read error dont know why so i manually sat it

	end = std::chrono::system_clock::now(); // Since this function is called when boosting has ended, set the end time point now
	std::chrono::duration<double> elapsed_seconds = end - start; // get the total elasped time soent boosting 
	boostUsed = (BoostConsumptionRate * elapsed_seconds.count()) * 100; // get the boost used by multiplying the consumtion rate by the time elasped the * 100 is purely for readablity/format purposes 

	if (boostUsed < 3) // Since the game mostly only does 3 to 4 boost intervals and only calls 1 or 2 when users only have 1 or 2 boost this will increase accurancy overall even thou in some isolated situtions it will decrease hey this thing aint perfect 
	{
		boostUsed = 3;
	}


	timeSpentBoosting = std::to_string(elapsed_seconds.count()); // this is for the log and display purposes not sure if there is any penalty for doing this 


	addToTotal(boostUsed);
}

float BoostyBoy::getCurrentBoostAmount()
{

	if (!gameWrapper->IsInOnlineGame() && !gameWrapper->IsInFreeplay()) { return 0; }
	else {

		float tempCurrentBoostAmount = 0;
		CarWrapper car = gameWrapper->GetLocalCar(); // Get the car and NULL check it 
		if (!car) { return 0; }

		BoostWrapper boostComponet = car.GetBoostComponent();
		if (!boostComponet) { return 0; }
		else {

			tempCurrentBoostAmount = boostComponet.GetCurrentBoostAmount();
			return tempCurrentBoostAmount;
		}
	
	}
}

void BoostyBoy::getBoostUsedOnline()
{
	CarWrapper car = gameWrapper->GetLocalCar();  
	if (!car) { return; }
	BoostWrapper carBoostComponent = car.GetBoostComponent();
	if (!carBoostComponent) { return; }

	if (!gameWrapper->IsInOnlineGame() && !gameWrapper->IsInFreeplay()) { return; }

	else if (currentBoostAmount == 0.00 && boostSwitchForOnlineGames == false)
	{
		newCalculateBoostUsed();
		boostSwitchForOnlineGames = true;
	}
}


void BoostyBoy::addToTotal(double boostUsed)
{
	if (!gameWrapper->IsInOnlineGame() && !gameWrapper->IsInFreeplay()) { return; }
	else {
		singleGameStats.totalBoostUsed += boostUsed;
	}
}

void BoostyBoy::gameStart()
{
	cvarManager->log("Match Has Started");
}

void BoostyBoy::setStatsToZero()
{
	 currentBoostAmount = 0.00;
	 boostUsed = 0.00;
	 timeSpentBoosting = "0.000000";
	 boostSwitchForOnlineGames = false; 
	 singleGameStats.bigPads = 0;
	 singleGameStats.smallPads = 0;
	 singleGameStats.totalBoostUsed = 0.00;
	 singleGameStats.winOrLoss = ' ';
	 
}

void BoostyBoy::addToAverage(stats tempStatStruct, std::deque<stats>&tempQForStatValues) 
{

	if (tempQForStatValues.size() == 25){
		tempQForStatValues.pop_front();
		tempQForStatValues.push_back(tempStatStruct);
	}
	else if (tempQForStatValues.size() < 25){
		tempQForStatValues.push_back(tempStatStruct);
	}
	else if (tempQForStatValues.size() > 25) // If this gets triggered something terrible has happened to the queue, this should never get triggered
	{
		while (tempQForStatValues.size() != 24)
		{
			tempQForStatValues.pop_front();
		}
	
		tempQForStatValues.push_back(tempStatStruct);
	}
}

BoostyBoy::stats BoostyBoy::getAverage(std::deque<stats>& tempQueue) 
{
	stats t{};

	if (tempQueue.size() == 0)
	{
		t.totalBoostUsed = 0.0000;
		t.bigPads = 0;
		t.smallPads = 0;
		t.winOrLoss = ' ';
	}
	else if (tempQueue.size() > 0)
	{
		std::deque<stats>::iterator it = --tempQueue.end();
		if (tempQueue.size() < amountOfGamesToTrack)
		{
			for (int x = 0; x != tempQueue.size(); ++x)
			{
				stats temp = *it;

				t.bigPads += temp.bigPads;
				t.smallPads += temp.smallPads;
				t.totalBoostUsed += temp.totalBoostUsed;
				
				--it;
			}
		}
		else 
		{
			for (int x = 0; x != amountOfGamesToTrack; ++x)
			{
				stats temp = *it;
				
				t.bigPads += temp.bigPads;
				t.smallPads += temp.smallPads;
				t.totalBoostUsed += temp.totalBoostUsed;
				
				--it;
			}
		}

		if (tempQueue.size() < amountOfGamesToTrack)
		{
			t.bigPads = t.bigPads / tempQueue.size();
			t.smallPads = t.smallPads / tempQueue.size();
			t.totalBoostUsed = t.totalBoostUsed / tempQueue.size();
		}
		else
		{
			t.bigPads = t.bigPads / amountOfGamesToTrack;
			t.smallPads = t.smallPads / amountOfGamesToTrack;
			t.totalBoostUsed = t.totalBoostUsed / amountOfGamesToTrack;
		
		}
	}

	return t;	
}

BoostyBoy::stats BoostyBoy::getAverage(char results, std::deque<stats> tempQueue) { // For getting averages in W's and in L's
	std::deque<stats>::iterator it;
	std::deque<stats> statsFromResults;
	
	stats t{};
	
	if (tempQueue.size() == 0)
	{
		t.totalBoostUsed = 0.0000;
		t.bigPads = 0;
		t.smallPads = 0;
		t.winOrLoss = ' ';
		
	}
	else if (tempQueue.size() > 0)
	{
		std::deque<stats>::iterator it = --tempQueue.end();
		if (tempQueue.size() < amountOfGamesToTrack)
		{
			for (int x = 0; x != tempQueue.size(); ++x)
			{
				stats temp = *it;
				if (temp.winOrLoss == results)
				{
					statsFromResults.push_back(temp);
				}
				--it;
			}
		}
		else
		{
			for (int x = 0; x != amountOfGamesToTrack; ++x)
			{
				stats temp = *it;
				if (temp.winOrLoss == results)
				{
					statsFromResults.push_back(temp);
				}

				--it;
			}
		}
		
		if (results == 'W') {
			wins = statsFromResults.size();
		}
		else if (results == 'L')
		{
			losses = statsFromResults.size();
		}
	}
	
	
	t = getAverage(statsFromResults);
	return t;

}

void BoostyBoy::readFromTextFile(std::string textFile, std::deque<stats>&tempQueue)
{
	std::ifstream input;
	input.open(textFile, std::ios::in);

		if (!input.is_open())
		{
			cvarManager->log("Could not open requested file..... Usually means first time startup so file hasn't been made yet");
			return;
		}
		stats temp;
		while (input >> temp){ // check here for reading problems
			tempQueue.push_back(temp);
		}

		input.close();
}

void BoostyBoy::saveToTextFile(std::string textFile, std::deque<stats>& tempQueue)
{
	std::ofstream output;
	output.open(textFile, std::ios::out);

	if (!output.is_open())
	{
		cvarManager->log("Could not open requested file.....");
		return;
	}
	std::deque<stats> temp = tempQueue;

	while (temp.size() != 0)
	{

		output << temp.front() << std::endl;
		temp.pop_front();
	}
}

void BoostyBoy::collectPad(BoostPickupWrapper bpw)
{
	float tempboostamount = getCurrentBoostAmount();

	if (currentBoostAmount >= tempboostamount) {return;} // This is the jank filtering, If this event happened and we now have more boost than a gametick ago then 99.9% it was us who picked up the pad.

	float boostAmountPickedUp = bpw.GetBoostAmount();

	if (boostAmountPickedUp > 12) { singleGameStats.bigPads++; return; }
	else { singleGameStats.smallPads++; return; }
	
};


/* Drawing stuff on screen settings */
void BoostyBoy::Render(CanvasWrapper canvas) 
{
	if (!renderEnabled) { return; }
	LinearColor colors; // color of text 
	colors.R = 255;
	colors.G = 255;
	colors.B = 0;
	colors.A = 255;
	canvas.SetColor(colors);

	CVarWrapper xLocCvar = cvarManager->getCvar("overlayXPosition");
	if (!xLocCvar) { return; }
	float xLoc = xLocCvar.getFloatValue();
	CVarWrapper yLocCvar = cvarManager->getCvar("overlayYPosition");
	if (!yLocCvar) { return; }
	float yLoc = yLocCvar.getFloatValue();
	CVarWrapper overlayScaleCvar = cvarManager->getCvar("overlayScale");
	if (!overlayScaleCvar) { return; }
	float overlayScale = overlayScaleCvar.getFloatValue();

	
	if (gameWrapper->IsInOnlineGame())
	{
		if (showBoostStats) {
			canvas.SetPosition(Vector2F{ xLoc, yLoc });
			canvas.DrawString("Boost Used: " + std::to_string(boostUsed), overlayScale, overlayScale, true);
			canvas.SetPosition(Vector2F{ xLoc, yLoc + (25 * overlayScale) });
			canvas.DrawString("Total Boost Used: " + std::to_string(singleGameStats.totalBoostUsed), overlayScale, overlayScale, true);
		}
		if (showPadStats) {
			canvas.SetPosition(Vector2F{ xLoc, yLoc + (50 * overlayScale) });
			canvas.DrawString("Total Big Pads: " + std::to_string(singleGameStats.bigPads), overlayScale, overlayScale, true);
			canvas.SetPosition(Vector2F{ xLoc, yLoc + (75 * overlayScale) });
			canvas.DrawString("Total Small Pads: " + std::to_string(singleGameStats.smallPads), overlayScale, overlayScale, true);
		}
	}
}


