/*
MIT License

Copyright (c) 2019-2020 KiD Fearless
Copyright (c) 2021-2022 rtldg

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#if defined _convar_class_included
 #endinput
#endif
#define _convar_class_included

// todo: track previous default values

static ArrayList _ConvarList;

enum struct convar_t
{
	char name[255];
	ConVar cvar;
	char description[512];
	char defValue[512];

	bool GetMin(float& input)
	{
		return this.cvar.GetBounds(ConVarBound_Lower, input);
	}
	bool GetMax(float& input)
	{
		return this.cvar.GetBounds(ConVarBound_Upper, input);
	}
	void SetMin(bool set, float& input = 0.0)
	{
		this.cvar.SetBounds(ConVarBound_Lower, set, input);
	}
	void SetMax(bool set, float& input = 0.0)
	{
		this.cvar.SetBounds(ConVarBound_Upper, set, input);
	}
}

methodmap Convar < ConVar
{
	public Convar(const char[] name, const char[] defaultValue, const char[] description = "",
					 int flags = 0, bool hasMin = false, float min = 0.0, bool hasMax = false, float max = 0.0, convar_t convar = {})
	{
		if(_ConvarList == null)
		{
			_ConvarList = new ArrayList(sizeof(convar_t));
		}

		ConVar cvar = CreateConVar(name, defaultValue, description, flags, hasMin, min, hasMax, max);

		convar_t savedValue;
		savedValue.cvar = cvar;
		strcopy(savedValue.name, sizeof(savedValue.name), name);
		strcopy(savedValue.description, 512, description);
		strcopy(savedValue.defValue, sizeof(savedValue.defValue), defaultValue);

		// Can't set default values :T
		savedValue.SetMin(hasMin, min);
		savedValue.SetMax(hasMax, max);

		// Have to do it this way instead of `convar = savedValue;` because SM 1.12 is a whiny bitch.
		convar.name = savedValue.name;
		convar.cvar = savedValue.cvar;
		convar.description = savedValue.description;
		convar.defValue = convar.defValue;

		_ConvarList.PushArray(savedValue);

		return view_as<Convar>(cvar);
	}

	public static bool CreateConfig(const char[] fileName = "", const char[] folder = "sourcemod", const bool clearWhenDone = true)
	{
		char localFolder[PLATFORM_MAX_PATH];
		FormatEx(localFolder, PLATFORM_MAX_PATH, "cfg/%s", folder);
		if(!DirExists(localFolder))
		{
			if(!CreateDirectory(localFolder, 755))
			{
				LogError("Error: Failed to create folder '%s'", localFolder);
			}
		}

		if(_ConvarList == null)
		{
			LogError("Error: No convars found. did you run .CreateConfig() before adding convars?");
			return false;
		}

		_ConvarList.Sort(Sort_Ascending, Sort_String);

		// Check if the file exists.
		char local[PLATFORM_MAX_PATH];
		if(fileName[0] == 0)
		{
			char pluginName[PLATFORM_MAX_PATH];
			GetPluginFilename(GetMyHandle(), pluginName, PLATFORM_MAX_PATH);

			ReplaceString(pluginName, PLATFORM_MAX_PATH, ".smx", "");
			ReplaceString(pluginName, PLATFORM_MAX_PATH, "\\", "/");

			int start = FindCharInString(pluginName, '/', true);

			FormatEx(local, PLATFORM_MAX_PATH, "cfg/%s/plugin.%s.cfg", folder, pluginName[start+1]);
		}
		else
		{
			FormatEx(local, sizeof(local), "cfg/%s/%s.cfg", folder, fileName);
		}
		bool fileExists = FileExists(local);
		if (!fileExists)
		{
			// Create first time file
			File file = OpenFile(local, "wt");
			if (file != null)
			{
				fileExists = true;

				// get the plugin name
				char pluginName[64];
				GetPluginFilename(GetMyHandle(), pluginName, 64);

				// Write warning
				file.WriteLine("// This file was auto-generated by KiD's Convar Class. Only plugin convars are allowed.");
				file.WriteLine("// ConVars for plugin \"%s\"\n\n", pluginName);

				// Loop through all of our convars
				for (int i = 0; i < _ConvarList.Length; ++i)
				{
					// get the current convar and description
					convar_t convar;
					_ConvarList.GetArray(i, convar);

					// don't write to file if flag is set
					if (convar.cvar.Flags & FCVAR_DONTRECORD != 0)
					{
						continue;
					}

					// format newlines as comments
					ReplaceString(convar.description, 512, "\n", "\n// ");

					// write the values and bounds to the file if they exist
					file.WriteLine("// %s", convar.description);

					file.WriteLine("// -");
					file.WriteLine("// Default: \"%s\"", convar.defValue);
					float x;
					if (convar.GetMin(x))
					{
						file.WriteLine("// Minimum: \"%02f\"", x);
					}
					if (convar.GetMax(x))
					{
						file.WriteLine("// Maximum: \"%02f\"", x);
					}
					file.WriteLine("%s \"%s\"\n", convar.name, convar.defValue);
				}
				// end with newline
				file.WriteLine("");
				delete file;
			}
			else
			{
				// writing failed, notify developer.
				char pluginName[64];
				GetPluginFilename(GetMyHandle(), pluginName, 64);
				LogError("Failed to auto generate config for %s at '%s', make sure the directory has write permission.", pluginName, local);
				if(clearWhenDone)
				{
					delete _ConvarList;
				}
				return false;
			}
		}
		// file already exists, just update the description and defaults
		else
		{
			// open the file for reading
			File file = OpenFile(local, "r");
			// create a stringmap to hold our current convars.
			StringMap convars = new StringMap();

			char line[512];
			int currentLine = 0;
			while(!file.EndOfFile() && file.ReadLine(line, 512))
			{
				TrimString(line);

				++currentLine;
				// check if the line contains a valid statement
				if(line[0] != '/' && line[0] != '\n' && line[0] != 0)
				{
					char buffers[2][512];
					// only convars should be in here. which should contain convar [space] value.
					if(ExplodeString(line, " ", buffers, 2, 512, true) == 2)
					{
						// remove any trailing whitespace. should be none
						TrimString(buffers[0]);
						TrimString(buffers[1]);
						// remove the quotes from the values
						StripQuotes(buffers[0]);
						StripQuotes(buffers[1]);

						// since convars are only ever strings we store it as strings.
						convars.SetString(buffers[0], buffers[1]);
					}
					else
					{
						// someone put something in there that shouldn't be. Yell at the dev for doing stupid stuff.
						LogError("Error exploding convar string: '%s' on line: %i", line, currentLine);
					}
				}
			}
			// close our file
			delete file;
			// yay duplicate code
			// rewrite the cfg with old convars removed and new convars added.
			/* Attempt to recreate it */
			DeleteFile(local);
			file = OpenFile(local, "wt");
			if (file != null)
			{
				fileExists = true;

				char pluginName[64];
				GetPluginFilename(GetMyHandle(), pluginName, 64);

				file.WriteLine("// This file was auto-generated by KiD's Convar Class. Only plugin convars are allowed.");
				file.WriteLine("// ConVars for plugin \"%s\"\n\n", pluginName);

				float x;
				for (int i = 0; i < _ConvarList.Length; ++i)
				{
					convar_t convar;
					_ConvarList.GetArray(i, convar);

					if (convar.cvar.Flags & FCVAR_DONTRECORD != 0)
					{
						continue;
					}

					ReplaceString(convar.description, 512, "\n", "\n// ");
					file.WriteLine("// %s", convar.description);

					file.WriteLine("// -");
					file.WriteLine("// Default: \"%s\"", convar.defValue);
					if (convar.GetMin(x))
					{
						file.WriteLine("// Minimum: \"%02f\"", x);
					}
					if (convar.GetMax(x))
					{
						file.WriteLine("// Maximum: \"%02f\"", x);
					}

					// only difference is that now we check for a stored value.
					char storedValue[512];
					if(convars.GetString(convar.name, storedValue, 512))
					{
						file.WriteLine("%s \"%s\"\n", convar.name, storedValue);
					}
					else
					{
						file.WriteLine("%s \"%s\"\n", convar.name, convar.defValue);
					}
				}
				file.WriteLine("");
				delete file;
			}
			else
			{
				char pluginName[64];
				GetPluginFilename(GetMyHandle(), pluginName, 64);
				LogError("Failed to auto generate config for %s, make sure the directory has write permission.", pluginName);

				if(clearWhenDone)
				{
					delete _ConvarList;
				}

				delete convars;
				return false;
			}

			delete convars;
		}
		if(fileExists)
		{
			ServerCommand("exec \"%s\"", local[4]);
		}
		if(clearWhenDone)
		{
			delete _ConvarList;
		}
		return true;
	}

	public static void AutoExecConfig(const char[] fileName = "", const char[] folder = "sourcemod", const bool clearWhenDone = true)
	{
		if(Convar.CreateConfig(fileName, folder, clearWhenDone))
		{
			AutoExecConfig(false, fileName, folder);
		}
	}

	public void Close()
	{
		delete _ConvarList;
	}
}
