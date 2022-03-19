#include "pch.h"
#include "BoostyBoy.h"


bool inDragMode = false;

	
	 
std::string BoostyBoy::GetPluginName() {
	return "BoostyBoy";
}

void BoostyBoy::SetImGuiContext(uintptr_t ctx) {
	ImGui::SetCurrentContext(reinterpret_cast<ImGuiContext*>(ctx));
}

// Render the plugin settings here
// This will show up in bakkesmod when the plugin is loaded at
//  f2 -> plugins -> BoostyBoy
void BoostyBoy::RenderSettings() {
	ImGui::TextUnformatted("Plugin made by GOTTABEFR3SH#0884 Hit me up on Discord with any comments or concerns.");
	ImGui::Spacing();
	ImGui::Spacing();
	
	
	if (ImGui::CollapsingHeader("Overlay Settings", ImGuiTreeNodeFlags_None))
	{
		overlaySettings();
	}

	if (ImGui::CollapsingHeader("Stats", ImGuiTreeNodeFlags_None))
	{
		statSettings();
		displayStatTable(statsFromPreviousMatches);
	}

}


void BoostyBoy::overlaySettings() {
	CVarWrapper enableRender = cvarManager->getCvar("render_Enable");
	if (!enableRender) { return; }
	bool enabled = enableRender.getBoolValue();
	if (ImGui::Checkbox("Turn On Overlay", &enabled)) {
		enableRender.setValue(enabled);
	}
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip("Toggles the on screen overlay");
	}
	CVarWrapper enableBoostStats = cvarManager->getCvar("boostStats_Enable");
	if (!enableBoostStats) { return; }
	bool enabledBS = enableBoostStats.getBoolValue();
	if (ImGui::Checkbox("Enable Boost Stats On Screen", &enabledBS)) {
		enableBoostStats.setValue(enabledBS);
	}
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip("Toggles on screen boost stats within a match.");
	}
	CVarWrapper enablePadStats = cvarManager->getCvar("padStats_Enable");
	if (!enablePadStats) { return; }
	bool enabledPS = enablePadStats.getBoolValue();
	if (ImGui::Checkbox("Enable Boost Pad Stats On Screen", &enabledPS)) {
		enablePadStats.setValue(enabledPS);
	}
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip("Toggles on screen boost pad stats within a match.");
	}
	CVarWrapper xLocCvar = cvarManager->getCvar("overlayXPosition");
	if (!xLocCvar) { return; }
	float xLoc = xLocCvar.getFloatValue();
	if (ImGui::SliderFloat("X Location", &xLoc, 0.0, 5000.0)) {
		xLocCvar.setValue(xLoc);
	}
	CVarWrapper yLocCvar = cvarManager->getCvar("overlayYPosition");
	if (!yLocCvar) { return; }
	float yLoc = yLocCvar.getFloatValue();
	if (ImGui::SliderFloat("Y Location", &yLoc, 0.0, 5000.0)) {
		yLocCvar.setValue(yLoc);
	}
	CVarWrapper overlayScaleCvar = cvarManager->getCvar("overlayScale");
	if (!overlayScaleCvar) { return; }
	float overlayScale = overlayScaleCvar.getFloatValue();

	if (ImGui::SliderFloat("Overlay Scale", &overlayScale, 0.00, 5.00)) {
		overlayScaleCvar.setValue(overlayScale);
	}
	ImGui::Checkbox("Drag Mode", &inDragMode);
	if (inDragMode) {
		if (ImGui::IsAnyWindowHovered() || ImGui::IsAnyItemHovered()) {
			// doesn't do anything if any ImGui is hovered over
			return;
		}
		// drag cursor w/ arrows to N, E, S, W
		ImGui::SetMouseCursor(2);
		if (ImGui::IsMouseDown(0)) {
			// if holding left click, move
			// sets location to current mouse position
			ImVec2 mousePos = ImGui::GetMousePos();
			xLocCvar.setValue(mousePos.x);
			yLocCvar.setValue(mousePos.y);
		}
	}
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip("Allows you to drag the interface with your mouse");
	}
}

void BoostyBoy::statSettings() {
	CVarWrapper amountOfGamesToTrackCvar = cvarManager->getCvar("amountOfGamesToTrack");
	if (!amountOfGamesToTrackCvar) { return; }
	static int games = amountOfGamesToTrackCvar.getIntValue();
	// Using the generic BeginCombo() API, you have full control over how to display the combo contents.
	// (your selection data could be an index, a pointer to the object, an id for the object, a flag intrusively
	// stored in the object itself, etc.)
	const char* items[] = { "5", "10", "15", "20", "25" };
	static int item_current_idx = 0; // Here we store our selection data as an index.
	const char* combo_preview_value = items[item_current_idx];  // Pass in the preview value visible before opening the combo (it could be anything)
	if (ImGui::BeginCombo("Amount Of Previous Games", combo_preview_value))
	{
		for (int n = 0; n < IM_ARRAYSIZE(items); n++)
		{
			const bool is_selected = (item_current_idx == n);
			if (ImGui::Selectable(items[n], is_selected))
			{
				item_current_idx = n;
				amountOfGamesToTrackCvar.setValue((n * 5) + 5);
			}
			// Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
			if (is_selected)
				ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}
	if (statsFromPreviousMatches.size() < amountOfGamesToTrack)
	{
		ImGui::Text("Averages From Past %i Games - Boost Used: %f - Big Pads: %i - Small Pads: %i", statsFromPreviousMatches.size(), averageStats.totalBoostUsed, averageStats.bigPads, averageStats.smallPads);
		ImGui::Separator();
		ImGui::Text("Averages From Wins - Games %i - Boost Used: %f - Big Pads: %i - Small Pads: %i", wins, averageWinStats.totalBoostUsed, averageWinStats.bigPads, averageWinStats.smallPads);
		ImGui::Separator();
		ImGui::Text("Averages From Losses Games %i - Boost Used: %f - Big Pads: %i - Small Pads: %i", losses, averageLossStats.totalBoostUsed, averageLossStats.bigPads, averageLossStats.smallPads);
	}
	else
	{
		ImGui::Text("Averages From Past %i Games - Boost Used: %f - Big Pads: %i - Small Pads: %i", amountOfGamesToTrack, averageStats.totalBoostUsed, averageStats.bigPads, averageStats.smallPads);
		ImGui::Separator();
		ImGui::Text("Averages From Wins - Games %i - Boost Used: %f - Big Pads: %i - Small Pads: %i", wins, averageWinStats.totalBoostUsed, averageWinStats.bigPads, averageWinStats.smallPads);
		ImGui::Separator();
		ImGui::Text("Averages From Losses Games %i - Boost Used: %f - Big Pads: %i - Small Pads: %i", losses, averageLossStats.totalBoostUsed, averageLossStats.bigPads, averageLossStats.smallPads);
		
	}

}


void BoostyBoy::displayStatTable(std::deque<stats> tempQueue){
	if (ImGui::TreeNode("Full Stats"))
	{
		ImGui::Columns(5, "mycolumns"); // 4-ways, with border
		ImGui::Separator();
		ImGui::Text("Game (Recent -> Least Recent)"); ImGui::NextColumn();
		ImGui::Text("Game Result"); ImGui::NextColumn();
		ImGui::Text("Total Boost Used"); ImGui::NextColumn();
		ImGui::Text("Big Pads"); ImGui::NextColumn();
		ImGui::Text("Small Pads"); ImGui::NextColumn();
		ImGui::Separator();

		static int selected = -1;
		
		if (tempQueue.size() == 0 ) { 
			ImGui::TreePop();	
			return; 
		}
		
		
		int x = 0;
		
		
		if (tempQueue.size() < amountOfGamesToTrack) {
			while(tempQueue.size() != 0)
			{
				char label[32];
				sprintf(label, "%d", x + 1);
				if (ImGui::Selectable(label, selected == x, ImGuiSelectableFlags_SpanAllColumns))
					selected = x;
				ImGui::NextColumn();
				std::string result;
				if (tempQueue.back().winOrLoss == 'W')
				{
					result = "Win";
				}
				else {
					result = "Loss";
				}
				double temp = tempQueue.back().totalBoostUsed;
				int intTemp = tempQueue.back().bigPads;
				int intTemp2 = tempQueue.back().smallPads;
				
				ImGui::Text("%s", result); ImGui::NextColumn();

				ImGui::Text("%f", temp); ImGui::NextColumn();
		
				ImGui::Text("%i", intTemp); ImGui::NextColumn();

				ImGui::Text("%i", intTemp2); ImGui::NextColumn();
				ImGui::Separator();

				tempQueue.pop_back();
				x++;
			}
		}
		else {
			for (int i = 0; i != amountOfGamesToTrack; i++)
			{
				char label[32];
				sprintf(label, "%d", x + 1);
				if (ImGui::Selectable(label, selected == x, ImGuiSelectableFlags_SpanAllColumns))
					selected = x;
				ImGui::NextColumn();
				std::string result;
				if (tempQueue.back().winOrLoss == 'W')
				{
					result = "Win";
				}
				else {
					result = "Loss";
				}
				
				
				double temp = tempQueue.back().totalBoostUsed;
				int intTemp = tempQueue.back().bigPads;
				int intTemp2 = tempQueue.back().smallPads;
				ImGui::Text("%s", result); ImGui::NextColumn();
				ImGui::Text("%f", temp); ImGui::NextColumn();

				ImGui::Text("%i", intTemp); ImGui::NextColumn();

				ImGui::Text("%i", intTemp2); ImGui::NextColumn();
				ImGui::Separator();

				tempQueue.pop_back();
				x++;

				
			}
		}
		ImGui::Columns(1);
		ImGui::TreePop();
	}
}

