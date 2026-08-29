/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Windows clipboard redirection
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

#ifndef FREERDP_CLIENT_CLIPRDR_WIN32_H
#define FREERDP_CLIENT_CLIPRDR_WIN32_H

#include <freerdp/api.h>
#include <freerdp/freerdp.h>
#include <freerdp/client/cliprdr.h>

#ifdef __cplusplus
extern "C"
{
#endif

	/** @brief native windows clipboard redirection.
	 *
	 * A self contained implementation of the cliprdr client side using the win32
	 * clipboard and OLE. In contrast to the generic implementation in
	 * client_cliprdr_file.c it supports copying files in both directions without
	 * FUSE, files are offered to the shell as an IDataObject with delayed
	 * rendering.
	 *
	 * The context runs its own message window in its own thread and only talks to
	 * the channel, so any client on windows can use it.
	 */
	typedef struct s_cliprdr_win32_context CliprdrWin32Context;

	/** @brief create a windows clipboard context and attach it to a cliprdr channel
	 *
	 * \param context the rdp context, used to access the channel list
	 * \param cliprdr the clipboard channel to attach to
	 *
	 * \return the new context or \b NULL in case of failure
	 */
	WINPR_ATTR_NODISCARD
	FREERDP_API CliprdrWin32Context* cliprdr_win32_context_new(rdpContext* context,
	                                                           CliprdrClientContext* cliprdr);

	/** @brief detach from the channel and free a windows clipboard context
	 *
	 * \param clipboard the context to free, may be \b NULL
	 * \param cliprdr the clipboard channel the context was attached to
	 */
	FREERDP_API void cliprdr_win32_context_free(CliprdrWin32Context* clipboard,
	                                            CliprdrClientContext* cliprdr);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_CLIENT_CLIPRDR_WIN32_H */
