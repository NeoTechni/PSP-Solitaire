	#include <stdlib.h>
	#include <stdio.h>
	#include <string.h>
	#include <psphprm.h>
	
	//begin DIR code
	#include <sys/stat.h>
	#include <unistd.h>
	//#include <ctype.h>
	#include <dirent.h>
	//end DIR code
	
#include <pspdebug.h> 
#include <png.h>
extern "C" {
#include "graphics.h" 
#include "common.h" //for DIR code
}
#define printf pspDebugScreenPrintf
#define MAXARGS 31

#define MAX_PATH 1024
#define RGB(r, g, b) ((r)|((g)<<8)|((b)<<16)) 

	//Store extention lists in arrays like this, it's easier, and let's the menu code use them as well
	char *file_ext[] = { ".mp3",  NULL }; //Supported Extention Array ".m3u",

	//DO NOT TOUCH
	char * current_dir_name = "";
	char * directory = "<DIR>";
	char * root = ".";
	char * isempty = "";
	char * str;
	const Color Transparent = 0x01010101;
	const int MAX_MENU_COUNT = 99; //Artificially imposed limit.
	
	//Special menu option type values
	//Any value >=0 implies it is a menu option with sub options. Uses a Zero indexed array in a | delimeted string
	const int NOOPTIONS = -1;	//Used for an option with no sub options (just show the whole text for the side)
	const int QUIT_MENU = -2;	//Used for an option that would end a loop (like an OK/Cancel button)
	const int MENU_TEXT = -3;	//Used for an option that when selected, would call up the onscreen keyboard (unimplemented)
	const int MENU_RANGE = -4;	//Used for an option who's suboptions would be a range of numbers, too long to have them all declared in a big string (unimplemented)
	const int UNSELECTABLE = -5;
	
struct region{
	Image* srchdc;
	int X, Y, Width, Height;
};
	
struct MenuItem{
	char* Text;
	char* Side;
	
	bool Checked;

	int OptionIndex;
	int OptionCount; //OptionCount = QUIT_MENU
};

struct Menu{
	int X;
	int Y;
	//int Width;
	int Height;
	int Column2;

	MenuItem *MenuList;//EMPTY ARRAY
	int MenuCount;
	
	bool isclean;
	bool hideselected;
	bool useskin;
	bool showindex;
	bool checkboxes;
	bool hideside;
	
	int Start;
	int SelectedItem; 
	int itemheight;
	
	int Whitespace;
};

bool NeedsRedraw = false;
Image* Skin;
region rChecked, rUnchecked, rUnselected, rSelected, background;
Color cSelected , cFont = 0x0, cFontSelected = 0x0;
Menu Soundtrackmenu, Submenu;

//prototypes
bool HandleRemote();

//wtf does it think is wrong with this?
int onScreen(Menu menu){
	int temp = 0, Number = menu.Height;
	while (Number > menu.itemheight) {
		Number = Number - menu.itemheight;
		temp++;
	}
	if (temp > menu.MenuCount) { temp = menu.MenuCount; }
	if (menu.Start + temp > menu.MenuCount) { temp = menu.MenuCount - menu.Start ; }
	return temp;
}

bool isVisible(Menu menu, int Index) {
	return (Index >= menu.Start) && (Index < menu.Start + onScreen(menu)) ;
}

void setSelectedItem(Menu& menu, int Index) {
	int Direction = 1;
	if (Index < menu.SelectedItem) { Direction = -1; }
	if (Index < 0) { Index = menu.MenuCount -1; }
	if (Index >= menu.MenuCount) { Index = 0; Direction = -1; }
	
	//do not let you select a menuitem labelled "-", or set to UNSELECTABLE
	while ( ( menu.MenuList[Index].Text == "-" ) || ( menu.MenuList[Index].Side == "-" ) || (menu.MenuList[Index].OptionCount == UNSELECTABLE) ){
		Index = Index + Direction;
		if (Index < 0) { Index = menu.MenuCount -1; }
		if (Index >= menu.MenuCount) { Index = 0; }
	}

	//if the new selecteditem is above the first visible menuitem, scroll up until it is the first visible menuitem
	if (Index < menu.Start) {
		menu.Start = Index;
	} else {
		//if the new selecteditem is below the last visible menuitem, scroll down until it is the last visible menuitem
		if (Index >= menu.Start + onScreen(menu)) {
			while(!isVisible(menu, Index)) {
				menu.Start++;
			}
		}
	}
	menu.isclean == false;
	menu.SelectedItem = Index;
}
bool menuControls(Menu& menu, SceCtrlData pad){
//PSP_CTRL_SQUARE PSP_CTRL_SELECT PSP_CTRL_START PSP_CTRL_UP PSP_CTRL_RIGHT PSP_CTRL_DOWN 
//PSP_CTRL_RTRIGGER PSP_CTRL_TRIANGLE PSP_CTRL_CIRCLE PSP_CTRL_CROSS PSP_CTRL_LEFT PSP_CTRL_LTRIGGER 
	bool didsomething = false;
	if (menu.MenuCount > 0) {
	
		//Triangle button - home
		if ((!didsomething) && (pad.Buttons & PSP_CTRL_TRIANGLE)) { didsomething = true;
			setSelectedItem(menu, 0);
		}
	
		
		//Circle button - end
		if ((!didsomething) && (pad.Buttons & PSP_CTRL_CIRCLE)) { didsomething = true;
			setSelectedItem(menu, menu.MenuCount -1);
		}
	
		//Square button - checkboxes
		if ((!didsomething) && (pad.Buttons & PSP_CTRL_SQUARE)) { didsomething = true;
			menu.MenuList[menu.SelectedItem].Checked = !menu.MenuList[menu.SelectedItem].Checked;
		}
	
		//DPAD up - previous item
		if ((!didsomething) && (pad.Buttons & PSP_CTRL_UP)) { didsomething = true;
			setSelectedItem(menu, menu.SelectedItem - 1);
		}
		
		//L trigger - page up
		if ((!didsomething) && (pad.Buttons & PSP_CTRL_LTRIGGER)) { didsomething = true;
			setSelectedItem(menu, menu.SelectedItem - onScreen(menu));
		}
		
		//DPAD down - next item
		if ((!didsomething) && (pad.Buttons & PSP_CTRL_DOWN)) { didsomething = true;
			setSelectedItem(menu, menu.SelectedItem + 1);
		}
		
		//R trigger - page down
		if ((!didsomething) && (pad.Buttons & PSP_CTRL_RTRIGGER)) { didsomething = true;
			setSelectedItem(menu, menu.SelectedItem + onScreen(menu));
		}
		
		//DPAD left - previous option
		if ((!didsomething) && (pad.Buttons & PSP_CTRL_LEFT)) { didsomething = true;
			if (menu.MenuList[menu.SelectedItem].OptionCount > -1) {
				menu.MenuList[menu.SelectedItem].OptionIndex--;
				if (menu.MenuList[menu.SelectedItem].OptionIndex < 0) {
					menu.MenuList[menu.SelectedItem].OptionIndex = menu.MenuList[menu.SelectedItem].OptionCount;
				}
			}
		}
		
		//DPAD right - next option
		if ((!didsomething) && (pad.Buttons & PSP_CTRL_RIGHT)) { didsomething = true;
			if (menu.MenuList[menu.SelectedItem].OptionCount > -1) {
				menu.MenuList[menu.SelectedItem].OptionIndex++;
				if (menu.MenuList[menu.SelectedItem].OptionIndex > menu.MenuList[menu.SelectedItem].OptionCount) {
					menu.MenuList[menu.SelectedItem].OptionIndex = 0;
				}
			}
		}
		
		//if ((!didsomething) && (pad.Buttons & PSP_CTRL_START)) { didsomething = true;}
		
		if (!didsomething) { HandleRemote(); }
		
		menu.isclean = !didsomething;
	}
	return didsomething;
}

void setupmenu(Menu& menu, int X = -1, int Y = -1,int Height = -1, int Column2 = -1, int itemheight = -1, bool hideselected = false, bool useskin = true, bool showindex = false, bool checkboxes = false, int Whitespace = -1, bool hideside = false) {
	//printf("Setting up the menu: "); int Width = -1, 
	if (Whitespace > -1) { menu.Whitespace = Whitespace; }
	if (X > -1) { menu.X = X; }
	if (Y > -1) {menu.Y = Y; }
	//if (Width > -1) {menu.Width = Width; }
	if (Height > -1) {menu.Height = Height; }
	if (Column2 > -1) {menu.Column2 = Column2; }
	if (itemheight > -1) {menu.itemheight = itemheight; }
	
	menu.hideselected = hideselected;
	menu.useskin = useskin;
	menu.showindex = showindex;
	menu.checkboxes = checkboxes;
	menu.hideside = hideside;
	
	menu.isclean = false;	
	menu.Start = 0;
	menu.SelectedItem = 0;
	//printf("Set up correctly\n");
}

void setupregion(region& Region, Image* srchdc, int X, int Y, int Width, int Height) {
	//printf("Setting up the region: ");
	Region.srchdc = srchdc;
	Region.X = X;
	Region.Y = Y;
	Region.Width = Width;
	Region.Height = Height;
	//printf("Set up correctly\n");
}

void DrawRegion(region Region, int X = 0, int Y = 0, int Width = -1, int Height = -1, int SrcX = 0, int SrcY = 0, Color color = Transparent){
	if (Width == -1)  {  Width =  Region.Width; }
	if (Height == -1) {  Height = Region.Height; }
	if (color == Transparent) {
		if (Region.srchdc) { blitImageToScreen(Region.X + SrcX, Region.Y + SrcY, Width, Height, Region.srchdc, X, Y); }
	} else {
		fillScreenRect(color,  X,  Y,  Width, Height);
	}
}

char* GetOption(char* Text, int OptionIndex = 0) {
	char * pch;
	//char * str;
	//str = Text;

	if (str) {free (str);}

	str = strdup ( Text);

	pch = strtok (str, "|");
	OptionIndex--;

	while ( (pch != NULL) && (OptionIndex > -1) ){
		pch = strtok (NULL, "|");
		OptionIndex--;
	}
	return pch;
}
	
void SetItem(Menu& menu, int Index, char* Text, char* Side, int options = 0, bool Checked = false) {
	/*
	if (menu.MenuList[Index].Text) {free (menu.MenuList[Index].Text);}
	if (menu.MenuList[Index].Side) {free (menu.MenuList[Index].Side);}
	menu.MenuList[Index].Text = strdup (Text);
	menu.MenuList[Index].Side = strdup (Side);
	*/
	
	menu.MenuList[Index].Text = Text;
	//sprintf(menu.MenuList[Index].Text, "%s", Score);
	
	menu.MenuList[Index].OptionCount = options;
	menu.MenuList[Index].Side = Side;
	menu.MenuList[Index].OptionIndex = 0;
	menu.MenuList[Index].Checked = Checked;
	
	if (options == 0) { menu.MenuList[Index].OptionIndex = -1; }
	if ( isVisible(menu, Index) ) { menu.isclean == false; }
}

int AddItem(Menu& menu, char* Text, char* Side, int options = -1, bool Checked = false) {
	if (MAX_MENU_COUNT > menu.MenuCount) {
		menu.MenuCount = menu.MenuCount + 1;
		menu.MenuList = (MenuItem*) realloc( menu.MenuList , sizeof(MenuItem) * menu.MenuCount); //REDIM PRESERVE
		SetItem (menu, menu.MenuCount - 1, Text, Side, options, Checked);
		return menu.MenuCount - 1;
	} else {
		return -1;
	}
}

void ClearItems(Menu& menu) {
	menu.SelectedItem = 0;
	if (menu.MenuCount > 0 ){
		menu.MenuCount = 0;
		//free (menu.MenuList);
		//menu.MenuList = (MenuItem*) realloc( menu.MenuList , sizeof(MenuItem) * menu.MenuCount); //REDIM PRESERVE //REDIM 
	}
	menu.isclean == false;
}

void RemoveItem(Menu& menu,int Index) {
	if (menu.MenuCount > 0) {
		if (menu.MenuCount == 1) {
			ClearItems(menu);
		} else {
			if (Index < menu.MenuCount - 1) {
                for (int temp = Index; temp < menu.MenuCount - 1; temp++) {
                    menu.MenuList[temp] = menu.MenuList[temp + 1];
				}
            }
			menu.MenuCount = menu.MenuCount - 1;
			menu.MenuList = (MenuItem*) realloc( menu.MenuList , sizeof(MenuItem) * menu.MenuCount);
			if (menu.SelectedItem >= Index){ menu.SelectedItem = menu.SelectedItem - 1 ;}
			if (isVisible(menu, Index)) { menu.isclean == false; }
		}
	}
}

void DrawMenuItem(Menu menu, int Index, int X, int Y){
	char* value;
	int oX = X;
	Color color = cFont, fillcolor = Transparent;
	bool Selected = false;
	//Font and box color
	if ( !menu.useskin ) { fillcolor = cSelected; }
	if ( (menu.SelectedItem == Index) && !menu.hideselected ){color = cFontSelected; Selected = true;}
	
	if (menu.MenuList[Index].Text == "-") {
		/*
		if ( menu.useskin ) { DrawRegion( rUnselected, X, Y); }
		drawLineScreen(X + menu.Whitespace, Y + rUnselected.Height / 2, X + rUnselected.Width - (menu.Whitespace*2), Y + rUnselected.Height / 2, color);
		*/
		
		//for (temp = X + menu.Whitespace; temp < X + rUnselected.Width - (menu.Whitespace*2) ; temp+=6) { //6 instead of 8 because the first and last pixels are empty
		//	printTextScreen (temp, Y, "-", color);
		//}
	} else {
	
	//Skin or selected color (hideslected)
	if ( (menu.MenuList[Index].Side != "-") && (menu.MenuList[Index].OptionCount != UNSELECTABLE) ) {
		if (Selected) {
			DrawRegion( rSelected, X, Y , menu.Column2, -1,0,0, fillcolor);
		} else {
			if ( menu.useskin ) { DrawRegion( rUnselected, X, Y , menu.Column2); }
		}
	}
		
	//Checkboxes
	if (menu.checkboxes) {
		if (menu.MenuList[Index].Checked) {
			DrawRegion( rChecked, X + menu.Whitespace, Y + menu.Whitespace);
		} else {
			DrawRegion( rUnchecked, X + menu.Whitespace, Y + menu.Whitespace);
		}
		X=X+9;
	}
	
	//Index numbers
	if (menu.showindex) {
		sprintf(value, "%d ", Index);
		if (value) {
			printTextScreen (X + menu.Whitespace, Y + menu.Whitespace, value, cFont);
			X=X+24;
		}
	}
	
	//Text (Left side)
	printTextScreen (X + menu.Whitespace, Y + menu.Whitespace, menu.MenuList[Index].Text, color);
	
	
	//Overdraw
	if ( (menu.MenuList[Index].Side != "-") && (menu.MenuList[Index].OptionCount != UNSELECTABLE)) {
		if (Selected) {
			DrawRegion ( rSelected, oX + menu.Column2, Y , rSelected.Width- menu.Column2, -1,menu.Column2,0, fillcolor);
		} else {
			DrawRegion(rUnselected, oX + menu.Column2, Y , rUnselected.Width- menu.Column2, -1,menu.Column2,0, fillcolor);
		}
	}
	
	//Right side Text
	if (!menu.hideside) {
		if (menu.MenuList[Index].Side != isempty) {
			X = oX + menu.Column2;
			if (menu.MenuList[Index].OptionCount > -1){
				printTextScreen (X + menu.Whitespace, Y + menu.Whitespace, GetOption(menu.MenuList[Index].Side, menu.MenuList[Index].OptionIndex), color);
			} else {
				printTextScreen (X + menu.Whitespace, Y + menu.Whitespace, menu.MenuList[Index].Side, color);
			}
		}
	}
	
	//Overdraw
	
	}
}

void DrawMenu(Menu menu){
	int temp, Y;
	Y = menu.Y;
	
	for (temp = menu.Start; temp < menu.Start + onScreen(menu); temp++) {
		DrawMenuItem( menu, temp, menu.X, Y);
		Y = Y + menu.itemheight;
	}
	menu.isclean = true;
}

int Askmenu(Menu& menu, SceCtrlData& pad){
	//clearScreen(0);
	bool Con = true;
	menu.isclean = false;
	DrawMenu(menu);
	flipScreen();
	//clearScreen(0);
	menu.isclean = false;
	
	if ( menu.MenuCount > 0 ) {
	
	sceCtrlReadBufferPositive(&pad, 1);
	while (pad.Buttons & PSP_CTRL_START) {
		doEvents();
		sceCtrlReadBufferPositive(&pad, 1);
	}
	
	while (Con) {
		sceCtrlReadBufferPositive(&pad, 1);
		if (menuControls(menu, pad)) {// (!menu.isclean){
			DrawMenu(menu);
			flipScreen();
			doEvents();
			doEvents();
		}
		if (pad.Buttons & PSP_CTRL_CROSS) {
			Con = (menu.MenuList[menu.SelectedItem].OptionCount != QUIT_MENU);
		}
		if (pad.Buttons & PSP_CTRL_START) {
			menu.SelectedItem = 0;
			Con = false;
		}
	}
	return menu.SelectedItem;
	
	} else { return -1; }
}




//CUSTOM SOUNDTRACK CODE

//The code below is modified from Exophase's gpSP GBA emulator.
char* CurrentWorkingDirectory(){//int chdir(const char * dirname);
	getcwd(current_dir_name, MAX_PATH); //get current working directory, put it in current_dir_name, buffer length of MAX_PATH
	return current_dir_name;
}

bool MoveItemUp(Menu& menu, int Item){
	MenuItem temp;
	int temp2;
	bool Switch = false;
	
	if (Item > 0) {
		temp2 = strcmp(menu.MenuList[Item].Side, menu.MenuList[Item-1].Side);
		if ( temp2 == -1){		//is the extention supposed to be higher on the list?
			Switch = true;
		} else { 				//if the extentions match, check if the filename is supposed to be higher on the list
			if (temp2 == 0) {
				Switch = (strcmp(menu.MenuList[Item].Text, menu.MenuList[Item-1].Text) == -1);	
			}
		}
	}
	
	if (Switch) {
		//temp = menu.MenuList[Item-1];
		//menu.MenuList[Item-1] = menu.MenuList[Item];
		//menu.MenuList[Item] = temp;
		return true;
	} else {
		return false;
	}
}

void Sortitem(Menu& menu, int Item){
	while (MoveItemUp(menu, Item)) {
		Item = Item - 1;
	}
}

//The code below is modified from Exophase's gpSP GBA emulator.
void menuFileList(Menu& menu, char* current_dir_name , char **wildcards, bool IncludeDIRS = true, bool IncludeFILES = true, bool checkDIRS = false, bool checkFILES = true){//Soundtrackmenu, "ms0:/PSP/MUSIC", file_ext, true, false
	DIR * current_dir;//POINTER TO A DIRECTORY
	struct dirent *current_file; //POINTER TO A FILE
    struct stat file_info; //FILE PROPERTIES
    char * file_name;//USED TO CONTAIN THE FILE NAME
	int file_name_length=0;//LENGTH OF THE FILE NAME
    char * ext_pos;//LOCATION OF THE "." IN THE FILE NAME
	bool doadd = false;
	char * side = isempty;
	bool check = false;
	//printf("Clearing menu: ");
	//ClearItems(menu);//EMPTY THE MENU OBJECT
	
	
	
	
	
	//printf("Success\nOpening directory: ");
	current_dir = opendir(current_dir_name);//OPEN THE DIRECTORY USING current_dir_name AS THEHANDLE
	
	//AddItem(menu, ".", directory, QUIT_MENU);
	//AddItem(menu, "..", directory, QUIT_MENU);
	
	if (current_dir){ //IF THE POINTER ACTUALLY POINTS TO A DIRECTORY
		//printf("Success\nOpening File: ");
		do {
		
			if(current_dir) {
				//printf("\nOpening File: ");
				current_file = readdir(current_dir);////SET current_file TO THE NEXT FILE IN THE FOLDER THAT current_dir POINTS TO (=FALSE IF NO MORE FILES)
				//printf("SUCCESS");
			} else {
				current_file = NULL;//PAST LAST FILE, SET current_file TO POINT TO NOTHING
				//printf("\nNULLIFIED");
			}
			
			if(current_file){ //current_file) {// IF THE POINTER ACTUALLY POINTS TO A FILE
				//printf("SUCCESS");
				file_name = current_file->d_name; //WTF IS d_name?
				file_name_length = strlen(file_name);//STORE LENGTH OF THE FILENAME
				ext_pos = strrchr(file_name, '.');
				doadd=false;
				stat(file_name, &file_info);
				
				//if (  >= 0 ) {
				if ( S_ISDIR(file_info.st_mode) ){ //IF FILE IS A DIRECTORY
					if (IncludeDIRS) { 
						doadd=true;
						check = checkDIRS;
						side = directory;
					} //ADD DIRECTORY TO MENU
				} else {
					//side = isempty;
					check = checkFILES;
					if (ext_pos && !doadd && IncludeFILES) {
						for(int i = 0; wildcards[i] != NULL; i++) {//FOR EACH GIVEN WILDCARD
							if (!doadd) {
								if(!strcasecmp((ext_pos), wildcards[i])) {//WILDCARD COMPARE WITH FILENAME?
									side = wildcards[i];//TRY TO FIX THE WEIRD NEW ERROR
									doadd=true;
								}
							}
						}
					}
				}
				
				if (doadd) { 
					AddItem(menu, strdup(file_name), side, QUIT_MENU, check); 
					//Sortitem(menu, menu.MenuCount-1);
				} //ADD FILE TO MENU  Sortitem(menu,
				//}
			}
			//printf("NEXT");	
		} while(current_file);
		//printf("\nCLOSING ");
		closedir(current_dir);//CLOSE THE DIRECTORY
		//SORT
		//END SORT
		//printf("SUCCESS");
	}
	//printf(" ENDFUNCTION");
}

bool PlayItem(int Index){
	char* test;
	bool retval = false;
	if (Index < Soundtrackmenu.MenuCount) {
		if (Soundtrackmenu.MenuList[Index].Side != directory) {
			test = strdup(Soundtrackmenu.MenuList[Index].Text);
			Soundtrackmenu.MenuList[Index].Checked = true;
			PREV_FILE = CURR_FILE;
			CURR_FILE = Index;
			LoadMP3(test);
			Play();
			retval = true;
			NeedsRedraw = true;
		}
	}
	return retval;
}

void NextTrack(){
	bool found = false;
	int temp = 0;
	
	if (RANDOM) {
		srand(time(NULL)); 
		while (!found){
			temp = rand()% Soundtrackmenu.MenuCount;
			if (Soundtrackmenu.MenuList[temp].Checked) {
				found = PlayItem(temp);
			}
		}
	} else {
		for (temp = CURR_FILE + 1; temp < Soundtrackmenu.MenuCount; temp++ ) {
			if (Soundtrackmenu.MenuList[temp].Checked) {
				if (!found){found = PlayItem(temp); }
			}
		}
		if (!found){
			for (temp = 0; temp < Soundtrackmenu.MenuCount; temp++ ) {
				if (Soundtrackmenu.MenuList[temp].Checked) {
					if (!found){found = PlayItem(temp); }
				}
			}
		}
	}
}

void PrevTrack(){
	bool found = false;
	int temp = 0;
	
	if (RANDOM) {
		if (PREV_FILE > -1) {
			PlayItem(PREV_FILE);
			PREV_FILE = -1;
		} else {
			NextTrack();
		}	
	} else {
		for (temp = CURR_FILE - 1; temp > 0; temp-- ) {
			if (Soundtrackmenu.MenuList[temp].Checked) {
				if (!found){found = PlayItem(temp); }
			}
		}
		if (!found){
			for (temp = Soundtrackmenu.MenuCount - 1; temp > 0; temp-- ) {
				if (Soundtrackmenu.MenuList[temp].Checked) {
					if (!found){found = PlayItem(temp); }
				}
			}
		}
	}

}

//	PSP_HPRM_PLAYPAUSE, PSP_HPRM_FORWARD, PSP_HPRM_BACK, PSP_HPRM_VOL_UP, PSP_HPRM_VOL_DOWN, PSP_HPRM_HOLD
bool HandleRemote(){
	u32 pad = 0;
	bool didsomething = false;
	
	if (MP3_EndOfStream() == 1) {
        NextTrack();
    } else {
	
	if (sceHprmIsRemoteExist() == 1) {
		if (sceHprmPeekCurrentKey(&pad) >= 0) {
		
			if ((!didsomething) && (pad & PSP_HPRM_PLAYPAUSE)) { didsomething = true;
				if (LOADED) { MP3_Pause(); }
			}
			
			if ((!didsomething) && (pad & PSP_HPRM_FORWARD)) { didsomething = true;
				if (Soundtrackmenu.MenuCount > 0) { NextTrack(); }
			}
			
			if ((!didsomething) && (pad & PSP_HPRM_BACK)) { didsomething = true;
				if (Soundtrackmenu.MenuCount > 0) { PrevTrack(); }
			}
			
			if (didsomething) {
				while (pad != 0) {
					doEvents();
					sceHprmPeekCurrentKey(&pad);
				}
			}
			
		}
	}
	
	}
	
	return didsomething;
}

//PSP_CTRL_SQUARE PSP_CTRL_SELECT PSP_CTRL_START PSP_CTRL_UP PSP_CTRL_RIGHT PSP_CTRL_DOWN 
//PSP_CTRL_RTRIGGER PSP_CTRL_TRIANGLE PSP_CTRL_CIRCLE PSP_CTRL_CROSS PSP_CTRL_LEFT PSP_CTRL_LTRIGGER 
void AskForFile(SceCtrlData& pad){
	bool didsomething = false, Con = true, isclean = false;
	
	//HOLD SELECT DOWN AND PRESS A COMMAND TO AVOID HAVING THE WHOLE MENU POPUP
	//FORCE MENU TO POPUP ANYWAY IF IT HASNT BEEN LOADED YET
	sceCtrlReadBufferPositive(&pad, 1);
	if (Soundtrackmenu.MenuCount > 0) {
		while ( pad.Buttons & PSP_CTRL_SELECT ) {
		
			if ((!didsomething) && (pad.Buttons & PSP_CTRL_START)) { didsomething = true;
				MP3_Pause();
			}
		
			if ((!didsomething) && (pad.Buttons & PSP_CTRL_RTRIGGER)) { didsomething = true;
				NextTrack();
			}
		
			if ((!didsomething) && (pad.Buttons & PSP_CTRL_LTRIGGER)) { didsomething = true;
				PrevTrack();
			}
		
			sceCtrlReadBufferPositive(&pad, 1);
		}
		doEvents();
		doEvents();
	}
	
	//NO PRELOAD COMMAND ISSUED, CALL THE MENU
	if (!didsomething) {
		//DrawRegion(background);
		//flipScreen();
		//DrawRegion(background);
	
		if (Soundtrackmenu.MenuCount == 0) {// ClearItems (Soundtrackmenu) ;
			chdir("ms0:/PSP/MUSIC");
			CurrentWorkingDirectory();
			menuFileList(Soundtrackmenu, "ms0:/PSP/MUSIC", file_ext);
			Soundtrackmenu.SelectedItem = 0;
		}
	
		//DrawMenu(Soundtrackmenu);
		//flipScreen();
		//isclean = true;
		
		didsomething = true;
		
		while (Con) {
		sceCtrlReadBufferPositive(&pad, 1);
		
		//Block out the ability to Check directories
		if ((!didsomething) && (pad.Buttons & PSP_CTRL_SQUARE)) { didsomething = true;
			if (Soundtrackmenu.MenuList[Soundtrackmenu.SelectedItem].Side != directory) {
				didsomething = false;
			}
		}
		
		//DPAD:Left - Page Up
		if ((!didsomething) && (pad.Buttons & PSP_CTRL_LEFT)) { didsomething = true;
			setSelectedItem(Soundtrackmenu, Soundtrackmenu.SelectedItem - onScreen(Soundtrackmenu));
			isclean=false;
		}

		//DPAD:Right - Page down
		if ((!didsomething) && (pad.Buttons & PSP_CTRL_RIGHT)) { didsomething = true;
			setSelectedItem(Soundtrackmenu, Soundtrackmenu.SelectedItem + onScreen(Soundtrackmenu));
			isclean=false;
		}

		//R trigger - Next Track
		if ((!didsomething) && (pad.Buttons & PSP_CTRL_RTRIGGER)) { didsomething = true;
			NextTrack();
			isclean=false;
		}
		
		//L trigger - Prev track
		if ((!didsomething) && (pad.Buttons & PSP_CTRL_LTRIGGER)) { didsomething = true;
			PrevTrack();
			isclean=false;
		}
		
		//X - Play file, browse directory/playlist
		if ((!didsomething) && (pad.Buttons & PSP_CTRL_CROSS)) { didsomething = true;
			if (Soundtrackmenu.MenuCount > 0 && Soundtrackmenu.SelectedItem > -1 && Soundtrackmenu.SelectedItem < Soundtrackmenu.MenuCount) {
				if (Soundtrackmenu.MenuList[ Soundtrackmenu.SelectedItem ].Side == directory) {
					if (strcmp(root, Soundtrackmenu.MenuList[Soundtrackmenu.SelectedItem].Text) == 0) {//fix root ignore problem
						chdir("ms0:/");
					} else {
						chdir(Soundtrackmenu.MenuList[Soundtrackmenu.SelectedItem].Text);
					}
					CurrentWorkingDirectory();
					ClearItems( Soundtrackmenu ) ;
					menuFileList(Soundtrackmenu, current_dir_name, file_ext);
					
					CURR_FILE = -1;
					PREV_FILE = -1;
					//DrawMenu(Soundtrackmenu);
					//flipScreen();
				} else { 
					/*
					test = strdup(Soundtrackmenu.MenuList[Soundtrackmenu.SelectedItem].Text);
					Soundtrackmenu.MenuList[Soundtrackmenu.SelectedItem].Checked = true;
					PREV_FILE = CURR_FILE;
					CURR_FILE = Soundtrackmenu.SelectedItem;
					LoadMP3(test);
					Play();
					*/
					PlayItem (Soundtrackmenu.SelectedItem);
				}
			}
			isclean=false;
		}
		
		//Select - Quit
		if ((!didsomething) && (pad.Buttons & PSP_CTRL_SELECT)) { didsomething = true;
			Soundtrackmenu.SelectedItem = 0;
			Con = false;
		}
		
		//Start - Play/Pause
		if ((!didsomething) && (pad.Buttons & PSP_CTRL_START)) { didsomething = true;
			Pause();
		}
		
		if ((!didsomething) && (pad.Buttons & PSP_CTRL_TRIANGLE)) { didsomething = true;
			DrawRegion(background);
			flipScreen();
			DrawRegion(background);
			Askmenu(Submenu, pad);
			RANDOM = Submenu.MenuList[0].OptionIndex == 1;
			Soundtrackmenu.isclean = false;
			isclean = false;
		}
		
		//Pass all other button presses to the menu handler (basically what at this point? Square, circle and triangle?
		if (!didsomething) {
			if (menuControls(Soundtrackmenu, pad)) {// (!menu.isclean){
				DrawMenu(Soundtrackmenu);
				isclean = true;
				if (LOADED) {
					if ( FILETITLEREAL ) { printTextScreen (240 - strlen(FILETITLEREAL) * 4 , 6, FILETITLEREAL, 0xFFFFFFFF); }
				}
				printTextScreen (58, 257, current_dir_name, 0xFFFFFFFF);
				flipScreen();
				didsomething = true;
			}
		}
		
		//Drawscreen
		if (!isclean||NeedsRedraw) {
			isclean = true;
			DrawRegion(background);
			flipScreen();
			DrawRegion(background);
			DrawMenu(Soundtrackmenu);
			NeedsRedraw=false;
			
			if (LOADED) {
				if ( FILETITLEREAL ) { printTextScreen (240 - strlen(FILETITLEREAL) * 4 , 6, FILETITLEREAL, 0xFFFFFFFF); }
			}
			
			printTextScreen (58, 257, current_dir_name, 0xFFFFFFFF);
			flipScreen();
		}
		
		//Delay and check for remote control keypresses
		if (didsomething) {
			doEvents();
			doEvents();
			didsomething = false;
		} else { HandleRemote(); }
		
		}
	}
}

void SetupSubMenu(){
	AddItem(Submenu, "Playmode",     					"Sequential|Random", 1); //0
	AddItem(Submenu, "-",     	  												isempty);//1
	AddItem(Submenu, "Done",   											isempty,	QUIT_MENU);//2
	AddItem(Submenu, "-",     	  													isempty);//3
	AddItem(Submenu, "L Trigger",						"Prev Track", 				UNSELECTABLE);//4
	AddItem(Submenu, "R Trigger",						"Next Track", 				UNSELECTABLE);//5
	AddItem(Submenu, "Start",							"Pause/Play", 				UNSELECTABLE);//6
	AddItem(Submenu, "Square",							"Queue/Unqueue",			UNSELECTABLE);//7
	AddItem(Submenu, "Cross",							"Play Selected",			UNSELECTABLE);//8
	AddItem(Submenu, "Left",							"Page Up",					UNSELECTABLE);//9
	AddItem(Submenu, "Right",							"Page Down",				UNSELECTABLE);//10
	AddItem(Submenu, "-",     	  													isempty);//11
	AddItem(Submenu, "Instead of pressing select to open this menu",    isempty,	UNSELECTABLE);//12
	AddItem(Submenu, "you can hold select and press a button",          isempty,	UNSELECTABLE);//13
	AddItem(Submenu, "(L or R trigger, or start) and this menu", 	    isempty,	UNSELECTABLE);//14
	AddItem(Submenu, "will be skipped. The remote works too.", 		    isempty,	UNSELECTABLE);//15
	AddItem(Submenu, "Thank you for playing my game :)", 		  	    isempty,	UNSELECTABLE);//16
}
