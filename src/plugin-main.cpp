/*
Plugin Name
Copyright (C) 2026 Mehdi Sheriff mehdirsheriff@gmail.com

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program. If not, see <https://www.gnu.org/licenses/>
*/

#include <obs-frontend-api.h>
#include <obs-module.h>
#include <plugin-support.h>

#include "CaptionsDock.h"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

namespace {
const char *kDockId = "SonioxLiveCaptionsDock";
}

bool obs_module_load(void)
{
	auto *dock = new CaptionsDock();
	obs_frontend_add_dock_by_id(kDockId, obs_module_text("LiveCaptions.DockTitle"), dock);

	obs_log(LOG_INFO, "plugin loaded successfully (version %s)", PLUGIN_VERSION);
	return true;
}

void obs_module_unload(void)
{
	obs_frontend_remove_dock(kDockId);
	obs_log(LOG_INFO, "plugin unloaded");
}
