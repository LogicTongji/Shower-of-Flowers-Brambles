return {
	version = 1,
	plugins = {
		{
			id = "china_anti_jap",
			channel = "china_anti_jap",
			module = "war_map_adapter",
			scope = "player_preferred",
			playerPriority = 100,
			fallbackPriority = 0,
			actionBudget = 64,
			refreshMode = "daily",
			maxConsecutiveErrors = 3,
			errorCooldownTicks = 16
		}
	}
}
