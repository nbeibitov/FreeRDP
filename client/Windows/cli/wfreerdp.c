/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Windows Client
 *
 * Copyright 2009-2011 Jay Sorg
 * Copyright 2010-2011 Vic Lee
 * Copyright 2010-2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <freerdp/config.h>

#include <winpr/windows.h>

#include <winpr/crt.h>
#include <winpr/cmdline.h>
#include <winpr/file.h>
#include <winpr/path.h>

#include <freerdp/freerdp.h>
#include <freerdp/constants.h>

#include <freerdp/client/file.h>
#include <freerdp/client/cmdline.h>
#include <freerdp/client/channels.h>
#include <freerdp/channels/channels.h>

#include "../resource/resource.h"

#include <wf_client.h>
#include <wf_defaults.h>

#include <shellapi.h>

static const char wf_window_remember[] = "window-remember";

static char* wf_window_position_file(const rdpSettings* settings)
{
	const char* config = freerdp_settings_get_string(settings, FreeRDP_ConfigPath);

	if (!config)
		return nullptr;

	return GetCombinedPath(config, "wfreerdp-window-position");
}

/** Restore the window position stored by the last run, /window-position wins if given. */
static void wf_window_position_load(rdpSettings* settings)
{
	char* file = wf_window_position_file(settings);

	if (!file)
		return;

	FILE* fp = winpr_fopen(file, "r");

	if (fp)
	{
		unsigned x = 0;
		unsigned y = 0;

		if (fscanf(fp, "%u %u", &x, &y) == 2)
		{
			if ((x <= UINT16_MAX) && (y <= UINT16_MAX))
			{
				(void)freerdp_settings_set_uint32(settings, FreeRDP_DesktopPosX, x);
				(void)freerdp_settings_set_uint32(settings, FreeRDP_DesktopPosY, y);
			}
		}

		fclose(fp);
	}

	free(file);
}

static void wf_window_position_save(const rdpSettings* settings, int x, int y)
{
	/* A minimized window reports -32000, do not store that */
	if ((x < 0) || (y < 0) || (x > UINT16_MAX) || (y > UINT16_MAX))
		return;

	const char* config = freerdp_settings_get_string(settings, FreeRDP_ConfigPath);

	if (config && !winpr_PathFileExists(config))
	{
		if (!winpr_PathMakePath(config, nullptr))
			return;
	}

	char* file = wf_window_position_file(settings);

	if (!file)
		return;

	FILE* fp = winpr_fopen(file, "w");

	if (fp)
	{
		(void)fprintf(fp, "%d %d\n", x, y);
		fclose(fp);
	}

	free(file);
}

static int wf_handle_option(const COMMAND_LINE_ARGUMENT_A* arg, void* custom)
{
	BOOL* remember = (BOOL*)custom;

	if (!arg || !arg->Name || !remember)
		return -1;

	if (strcmp(arg->Name, wf_window_remember) == 0)
		*remember = (arg->Value == BoolValueTrue);

	return 0;
}

INT WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	BOOL remember = FALSE;
	COMMAND_LINE_ARGUMENT_A wf_args[] = {
		{ wf_window_remember, COMMAND_LINE_VALUE_BOOL, nullptr, BoolValueFalse, nullptr, -1, nullptr,
		  "Restore the window position of the previous run and store it again on exit" },
		{ nullptr, 0, nullptr, nullptr, nullptr, -1, nullptr, nullptr }
	};
	int status;
	HANDLE thread;
	wfContext* wfc;
	DWORD dwExitCode;
	rdpContext* context;
	rdpSettings* settings;
	LPWSTR cmd;
	char** argv = nullptr;
	RDP_CLIENT_ENTRY_POINTS clientEntryPoints = WINPR_C_ARRAY_INIT;
	int ret = 1;
	int argc = 0;
	LPWSTR* args = nullptr;

	WINPR_UNUSED(hInstance);
	WINPR_UNUSED(hPrevInstance);
	WINPR_UNUSED(lpCmdLine);
	WINPR_UNUSED(nCmdShow);

	RdpClientEntry(&clientEntryPoints);
	context = freerdp_client_context_new(&clientEntryPoints);

	if (!context)
		return -1;

	cmd = GetCommandLineW();

	if (!cmd)
		goto out;

	args = CommandLineToArgvW(cmd, &argc);

	if (!args || (argc <= 0))
		goto out;

	argv = calloc((size_t)argc, sizeof(char*));

	if (!argv)
		goto out;

	for (int i = 0; i < argc; i++)
	{
		int size = WideCharToMultiByte(CP_UTF8, 0, args[i], -1, nullptr, 0, nullptr, nullptr);
		if (size <= 0)
			goto out;
		argv[i] = calloc((size_t)size, sizeof(char));

		if (!argv[i])
			goto out;

		if (WideCharToMultiByte(CP_UTF8, 0, args[i], -1, argv[i], size, nullptr, nullptr) != size)
			goto out;
	}

	freerdp_client_warn_deprecated(argc, argv);

	settings = context->settings;
	wfc = (wfContext*)context;

	if (!settings || !wfc)
		goto out;

	status = freerdp_client_settings_parse_command_line_ex(
	    settings, argc, argv, FALSE, wf_args, ARRAYSIZE(wf_args) - 1, wf_handle_option, &remember);
	if (status)
	{
		ret = freerdp_client_settings_command_line_status_print_ex(settings, status, argc, argv,
		                                                           wf_args);
		goto out;
	}

	AddDefaultSettings(settings);

	/* An explicit /window-position takes precedence over the stored one */
	if (remember && (freerdp_settings_get_uint32(settings, FreeRDP_DesktopPosX) == UINT32_MAX))
		wf_window_position_load(settings);

	if (freerdp_client_start(context) != 0)
		goto out;

	thread = freerdp_client_get_thread(context);

	if (thread)
	{
		if (WaitForSingleObject(thread, INFINITE) == WAIT_OBJECT_0)
		{
			GetExitCodeThread(thread, &dwExitCode);
			ret = (int)dwExitCode;
		}
	}

	if (remember)
		wf_window_position_save(settings, wfc->client_x, wfc->client_y);

	if (freerdp_client_stop(context) != 0)
		goto out;

out:
	freerdp_client_context_free(context);

	if (argv)
	{
		for (int i = 0; i < argc; i++)
			free(argv[i]);

		free(argv);
	}

	LocalFree(args);
	return ret;
}

#ifdef WITH_WIN_CONSOLE
int main()
{
	return WinMain(nullptr, nullptr, nullptr, 0);
}
#endif
