/*
 Copyright (C) 2011 Peter Szucs

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "WebEmberRunner.h"

#include "WebEmberMessageQueue.h"
#include "WebEmber.h"

#include <iostream>
#include <string>
#include <fstream>
#include <vector>

bool WebEmberRunner::sRunning = false;

WebEmberRunner::WebEmberRunner(WebEmberMessageQueueWeakPtr queue) :
	mQueue(queue)
{
}
WebEmberRunner::~WebEmberRunner()
{
	quitEmber();

}
std::string WebEmberRunner::getPrefix()
{
	std::string prefix;
#ifdef _WIN32
	const int buffsize = MAX_PATH;
	char buffer[buffsize];
	//check the location of the current DLL, we need to know the output filename.
	//WebEmber.dll should be next to npWebEmber.dll
	GetModuleFileNameA(GetModuleHandleA("npWebEmber.dll" ), buffer, buffsize);
	//trim to the last \ or / character
	std::string libpath(buffer);
	int pos = libpath.find_last_of("\\/");
	if(pos != std::string::npos) {
		libpath.resize(pos-1);
	}

	SetDllDirectoryA(libpath.c_str());
	SetCurrentDirectoryA(libpath.c_str());

	libpath += '\\';

	//normally the browsers are disabling verbose popup messages from the OS on errors,
	//but to get more info on what is the missing DLL when linking recursively, we need to enable this.
	UINT browserErrorMode = GetErrorMode();
	SetErrorMode(0);

#elif defined(__APPLE__)
	prefix = "@loader_path/..";
#else

	//check for PREFIX macro.
#ifdef PREFIX
	prefix = PREFIX;
#endif
	const char* path;
#ifdef USE_X11
	//check "~/.ember/webember.path".
	//NOTE: ~ needs to be replaced with HOME in some versions of firefox.
	//after that, read the first line of the file and interpret it as a path.

	path = getenv("HOME");
	if(path != 0){
		std::string pf(path);
		pf = "~/.ember/webember.path";
		std::ifstream ifs(pf.c_str());
		if(ifs.is_open()){
			pf.clear();
			std::getline(ifs, pf);
			ifs.close();
			if(!pf.empty()){
				prefix = pf;
			}
		}
	}
#endif

	//check for WEBEMBER_PREFIX environment variable.
	path = getenv("WEBEMBER_PREFIX");
	if (path != NULL) {
		prefix = path;
	}
#endif
	
#ifdef USE_X11
	//replace '~' character at the beginning with HOME env var if availible. 
	if(prefix[0] == '~'){
		path = getenv("HOME");
		if(path != 0){
			prefix.replace(0,1,path);
		}
	}
#endif
	
	return prefix;
}
int WebEmberRunner::runEmber(std::string windowhandle)
{

	FBLOG_INFO("WebEmberRunner::runEmber", "Starting Ember.");
	std::string prefix(getPrefix());
#ifdef USE_X11
	std::string libpath(prefix + "/lib");
	//change directory for rpath compatibility.
	chdir(libpath.c_str());
#endif
	if (mLinker.link(prefix.c_str()) == 0) {
		int ret = mLinker.callRunEmber(windowhandle.c_str(), prefix.c_str());
		if (ret) {
			FBLOG_INFO("WebEmberRunner::runEmber", "Shut down WebEmber with error code: " << ret);
		} else {
			FBLOG_INFO("WebEmberRunner::runEmber", "Shut down WebEmber normally.");
		}
	}
	
	//send quit to message queue
	{
		WebEmberMessageQueuePtr queue(mQueue.lock());
		if (queue) {
			queue->stop();
		}
	}
	
	mLinker.unlink();

	sRunning = false;
	return 0;
}
void WebEmberRunner::quitEmber()
{
	mLinker.callQuitEmber();
}

int WebEmberRunner::emberThread(WebEmberRunnerPtr runner, std::string windowhandle)
{
	return runner->runEmber(windowhandle);
}
