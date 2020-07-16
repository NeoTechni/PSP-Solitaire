#include <stdlib.h>
#include <pspkernel.h>
#include <pspdebug.h> 
#include <pspdisplay.h>
#include <pspctrl.h>
#include <pspgu.h>
#include <png.h>
#include <stdio.h>
#include <psprtc.h>
#include <math.h>

extern "C" {
#include "graphics.h" 
}

#include "Array.cpp"
#include "customsoundtracks.cpp"
#include "menu.cpp"
 
PSP_MODULE_INFO("Solitaire", 0, 1, 1); 
#define printf pspDebugScreenPrintf

//PROTOTYPING
void SelectCards();

const double Pi = 3.14159265358979;
const int TopRow = 2, MiddleRow = 64, BottomRow = 209;
int MaxRotations = 3, Rotations = 0, MaxCards = 3, ScoringMethod = 0, TimePassed = 0, BufferPile =-1, Score = 0;
bool TimedGame = false, HasLost = false;
int ScoreX = 470, ScoreYdown = 272, ScoreYup = 262, ScoreY = 272;
Menu Mainmenu, Losemenu, Winmenu; //DrawMenu(Mainmenu);

double	tickRes = 0, oldtime, newtime, temptime;
u64 OldTime, NewTime;

double CurrentTime(){
	u64 ttime;
	sceRtcGetCurrentTick( &ttime );	
	return ttime / tickRes;
}

void dooldtime(){
	//time(&OldTime);        
	sceRtcGetCurrentTick(&OldTime);
	oldtime = OldTime / tickRes;
}

int TextWidth(char* Text){
	return strlen(Text) * 8;
}

/* Exit callback */
int exit_callback(int arg1, int arg2, void *common) {
          sceKernelExitGame();
          return 0;
}

/* Callback thread */
int CallbackThread(SceSize args, void *argp) {
          int cbid;

          cbid = sceKernelCreateCallback("Exit Callback", exit_callback, NULL);
          sceKernelRegisterExitCallback(cbid);

          sceKernelSleepThreadCB();

          return 0;
}

/* Sets up the callback thread and returns its thread id */
int SetupCallbacks(void) {
          int thid = 0;

          thid = sceKernelCreateThread("update_thread", CallbackThread, 0x11, 0xFA0, 0, 0);
          if(thid >= 0) {
                    sceKernelStartThread(thid, 0, 0);
          }

          return thid;
} 

void setupdefaultskin(){
	char buffer[200];
	sprintf(buffer, "skin.png");
	Skin = loadImage(buffer); 
	if (Skin) {
		setupregion(rUnchecked,  Skin,   0, 272,   8,   8);
		setupregion(rChecked,    Skin,   0, 279,   8,   8);
		setupregion(rUnselected, Skin,  57, 272, 376,  12);
		setupregion(rSelected,   Skin,  57, 284, 376,  12);
		setupregion(background,  Skin,   0,   0, 480, 272);
		setupmenu(Mainmenu, 		57,  0, 272, 300, 13, false, true, false, false, 2);
		setupmenu(Losemenu, 		57,  0, 272, 300, 13, false, true, false, false, 2);
		setupmenu(Winmenu,  		57,  0, 272, 300, 13, false, true, false, false, 2);
		
		setupmenu(Soundtrackmenu,  	58, 24, 226, 330, 13, false, true, false, true, 2);
		setupmenu(Submenu, 			57, 24, 272, 250, 13, false, true, false, false, 2);
		SetupSubMenu();
	}
	
	AddItem(Mainmenu, "Draw",     "3|1", 1); //0
	AddItem(Mainmenu, "Scoring",  "Standard|Vegas|None", 2);//1
	AddItem(Mainmenu, "Timed Game",  "No|Yes", 1);//2
	AddItem(Mainmenu, "-",     	  "");//3
	AddItem(Mainmenu, "New Game", "", QUIT_MENU);//4
	AddItem(Mainmenu, "Cancel",   "", QUIT_MENU);//5
	//AddItem(Mainmenu, "-",     	  "");//6
	//AddItem(Mainmenu, "Quit Game",   "", QUIT_MENU);//7
	
	AddItem(Losemenu, "Game Over. Press X to start a new game", "", QUIT_MENU);//0
	AddItem(Winmenu, "You won. Press X to start a new game", "", QUIT_MENU);//0
	
	Mainmenu.Y = 136 - ( Mainmenu.MenuCount * 6 );
	Losemenu.Y = 130;
	Winmenu.Y = 130;
}

//Converts Degrees to Radians
double DegreesToRadians(double Degrees) {
    return Degrees * (Pi / 180);
}

void initsolitaire(){
	AddPile (  2, TopRow,     Deck ); //'0 Deck
    AddPile (  74, TopRow,    Dealt); //'1 3 dealt cards
    
    AddPile ( 304, TopRow,    Aces ); //'2 ace pile 1
    AddPile ( 348, TopRow,    Aces ); //'3 ace pile 2
    AddPile ( 392, TopRow,    Aces ); //'4 ace pile 3
    AddPile ( 436, TopRow,    Aces ); //'5 ace pile 4
    
    AddPile (   2, MiddleRow, Cards);      //'6 card pile 1
    AddPile (  74, MiddleRow, Cards);      //'7 card pile 2
    AddPile ( 147, MiddleRow, Cards);      //'8 card pile 3
    AddPile ( 220, MiddleRow, Cards);      //'9 card pile 4
    AddPile ( 293, MiddleRow, Cards);      //'10 card pile 5
    AddPile ( 366, MiddleRow, Cards);      //'11 card pile 6
    AddPile ( 436, MiddleRow, Cards);      //'12 card pile 7
    
    AddPile (   2, TopRow,    Invisible);  //13 discard pile
    AddPile (   2, BottomRow, Horizontal); //'14 Selected cards
	
	dooldtime(); 
	setupdefaultskin();
	initGraphics(); 
}

int AcePile(int Suite){
	int temp=0,temp2=-1,temp3 = -1;
	for (temp=2; temp < 6; temp++){
	
		if (temp3 == -1) {//temp2 is the location of the ace pile with the same suite
			if (PileList[temp].CardCount > 0) {
				if (PileList[temp].CardList[0].Suite == Suite) {
					temp3 = temp;
				}
			}
		} 
		
		if (temp2 == -1){//temp2 is the location of the first empty ace pile
			if (PileList[temp].CardCount == 0) {
				temp2 = temp;
			}
		}
	}
	if (temp3 > -1) { temp2 = temp3; }
	return temp2;
}

int AceNextCard(int Suite){
	int temp=0;
	temp = AcePile(Suite);
	if ( PileList[temp].CardCount == 0 ) {
		return 14;
	} else {
		return PileList[temp].CardCount + 1;
	}
}

bool CanPlaceInAces(){
	bool temp=false;
	if (PileList[14].CardCount > 0) {
		temp = (AceNextCard( PileList[14].CardList[ PileList[14].CardCount - 1 ].Suite ) == PileList[14].CardList[ PileList[14].CardCount - 1 ].Value )   ;
	}
	return temp;
}

int findapile(int Value, int Suite){
	int temp, retval = BufferPile;
	for (temp=6; temp < 13; temp++){
		if (Value == 13){
			if (PileList[temp].CardCount == 0) {
				retval = temp;
				temp = 13;
			}
		} else {
			if ( temp != BufferPile ) {
				if (PileList[temp].CardList[PileList[temp].CardCount-1].Value == Value + 1) {
					if ( GetColor(PileList[temp].CardList[PileList[temp].CardCount-1].Suite) != GetColor(Suite) ) {
						if (PileList[temp].CardList[PileList[temp].CardCount-1].Face) {
							retval = temp;
							temp = 13;
						}
					}
				}
			}
		}
	}
	return retval;
}

void DumpToAces(){
	bool temp=false, moved=false;
	temp = CanPlaceInAces();
	while(temp){
		moved=true;
		MoveCard( PileList[14], PileList[14].CardCount - 1, PileList[ AcePile( PileList[14].CardList[ PileList[14].CardCount - 1 ].Suite ) ] );
		temp = CanPlaceInAces();
	}
	if (!moved && PileList[14].CardCount > 0){
		SelectedPile = findapile(PileList[14].CardList[0].Value, PileList[14].CardList[0].Suite);
		SelectedCard = PileList[SelectedPile].CardCount - 1;
		SelectCards();
	}
}


int ScorePoints(int Vegas, int Standard){
	int temp = 0;
    if (ScoringMethod == 1) { temp = Vegas; }
	if (ScoringMethod == 0) { temp = Standard; }
	ScoreY = ScoreYup;
	return temp;
}

void newgame(){
	int temp, temp2;
	EmptyList();
	ShuffleDeck (PileList[0]);
	
	//printf("%d cards in the deck! \n", PileList[0].CardCount);
	
    for (temp = 0; temp < 7; temp++ ) {
        for (temp2 = 0; temp2 <= temp; temp2++) {
            MoveCard(PileList[0], PileList[0].CardCount - 1, PileList[6 + temp]);
            PileList[6 + temp].CardList[PileList[6 + temp].CardCount - 1].Face = (temp2 == temp);
        }
    }
	
	if ((ScoringMethod == 0) || (ScoringMethod == 2))  { MaxRotations = 32767; }
	if (ScoringMethod == 1)  { MaxRotations = MaxCards; }
	
	SelectedPile = 0;
	SelectedCard = 0;
	Rotations = MaxRotations;
	TimePassed = 0;
	HasLost = false;
	Score = Score - ScorePoints(52,0);
	
	dooldtime();
	AUTOdrawscreen(); 
}

void SolitaireScore(int srcpile, int destPile) {
    switch (PileList[srcpile].Style){
        case Dealt:
            if (PileList[destPile].Style == Aces) {
                Score = Score + ScorePoints(5, 10);
            } else {
                Score = Score + ScorePoints(0, 5);
            }
		break;
        case Aces:
			Score = Score + ScorePoints(-5, -10);
		break;
        case Cards: 
			if (PileList[destPile].Style == Aces) { Score = Score + ScorePoints(5, 10); }
		break;
		default:
		
		break;
    }
	ScoreY = ScoreYup;
}

void UnflipACard() {
    if (ScoringMethod == 2) { Score = Score + 5 ; }
	ScoreY = ScoreYup;
}

void OneSecondPassed() {
    TimePassed = TimePassed + 1;
	dooldtime();
	if (TimedGame) {
		if ( (TimePassed % 10 == 0) && (ScoringMethod == 2) && (Score > 0) ) {
			Score = Score -2;
			ScoreY = ScoreYup;
		}
	}
	
}

void CompletedRotation(){
	if ( ( PileList[0].CardCount + PileList[13].CardCount ) > 0 ) {//if the deck and discard piles are not empty
		if ( (ScoringMethod == 2) && (Score > 0) ) { Score = Score - 20; }
		ScoreY = ScoreYup;
	}
}

void Deal3Cards(int Amount = 3){
	int temp;
	if (PileList[14].CardCount == 0 ) {
	//empty the dealt cards into the invisible discard pile
	while (PileList[1].CardCount > 0) {
        MoveCard ( PileList[1], 0, PileList[13] );
    }
	
	//check if the deck is empty
	if (PileList[0].CardCount > 0) {
		//deal 3 cards, if the deck has the cards to deal
        for (temp = 0; temp < Amount; temp++) {
            if (PileList[0].CardCount > 0) {
                MoveCard ( PileList[0], PileList[0].CardCount - 1, PileList[1] );
            }
        }
    } else {
		Rotations = Rotations - 1;
		if (Rotations == 0) {
			HasLost = true;//that was the last rotation. you lose
			doEvents();
			Askmenu(Losemenu, pad);
			newgame();
		} else {
			CompletedRotation();
			while (PileList[13].CardCount > 0) {
				MoveCard ( PileList[13], PileList[13].CardCount - 1, PileList[0] );
			}
		}
	}
	}
}

bool HasWon(){
    int temp;
	bool buffer;
    buffer = true;
    for (temp = 2; temp <= 5; temp++){
        if (PileList[temp].CardCount < 13) { buffer = false; }
    }
    return buffer;
}

int GetCardY(int Factor,int Angle){
    return (int) (sin(DegreesToRadians(Angle)) * Factor);
}


void DropCard(CardPile& Pile){
	Card tempcard;
	int temp, temp2, Factor, Angle, Delay, increment;
	double loss;
    if (Pile.CardCount > 0) {
		//SeedRandom();
		increment = RandomNumber(10, 1);
        tempcard = Pile.CardList[Pile.CardCount - 1];
        DeleteCard (Pile, Pile.CardCount - 1);
        Factor = 211 - Pile.Y;
        Angle = 90;
		loss = (0.25 + (RandomNumber(50)/100) );
        Delay = RandomNumber(100);// 'Randomize speed
        for (temp = Pile.X; temp > -FullCardWidth; temp = temp - RandomNumber(7, 2)) { //Randomize horizontal distance between redraws
            Angle = Angle + increment;
            if (Angle >= 180) { Angle = 0; }
            temp2 = GetCardY(Factor, Angle);
            if (temp2 <= 0) { Factor = (int) (Factor * loss); } // 'randomize % of energy lost per bounce (in between 50% and 75%)   return (rand()%(Max-Min) ) + Min;
            DrawCardStyle (srcLeft, srcRight, temp, 211 - temp2, FullFront, tempcard.Value, tempcard.Suite);
            flipScreen();
			DrawCardStyle (srcLeft, srcRight, temp, 211 - temp2, FullFront, tempcard.Value, tempcard.Suite);
            //Sleep Delay
            //doEvents();
        }
	}
}

void WINGAME(){
    static bool IsWinning = false;
	int temp, temp2 = 2;
    if (!IsWinning) {// Then Exit Sub 'prevents multiple instances as theyd interfere
		IsWinning = true;
		AUTOdrawscreen();
		AUTOdrawscreen();
		SeedRandom();
		for (temp = 1; temp <= 52; temp++){
			DropCard (PileList[temp2]);
			temp2++;
			if (temp2 == 6) { temp2 = 2; }
		}
		IsWinning = false;
		doEvents();
		Askmenu(Winmenu, pad);
		newgame();	
	}
}

bool CanPlaceStack(){
	bool temp = false;
	if ( (PileList[14].CardCount > 0) && (SelectedPile > 1) && (SelectedPile < 13) ) {// Then 'if there is a selected card
		if (SelectedPile < 6) {//Case 2, 3, 4, 5 'ace piles
            //'Can only place one card at a time
            if ( PileList[14].CardCount == 1) {
				//'is the ace pile empty and the selected card is an ace then
                if ( PileList[SelectedPile].CardCount == 0 ) {
                    if (PileList[14].CardList[0].Value == 14) { temp = true;}
                } else {
					//'if the selected card is 1 higher than the last card on the pile, and matches the suite then
					if ( PileList[14].CardList[0].Suite == PileList[SelectedPile].CardList[0].Suite) {
						if ( PileList[14].CardList[0].Value == PileList[SelectedPile].CardList[PileList[SelectedPile].CardCount - 1].Value + 1 ) { temp = true; }
						if ( (PileList[14].CardList[0].Value == 2) && (PileList[SelectedPile].CardList[PileList[SelectedPile].CardCount - 1].Value == 14) ) { temp = true; }
                    } else {
					
					}
                }
            }
        } else { //Case 6, 7, 8, 9, 10, 11, 12 'card piles
            //'can place multiple cards at a time
            if (PileList[SelectedPile].CardCount == 0) {
                //'if pile is empty, and the selected card is a king then
                if ( PileList[14].CardList[0].Value == 13 ) { temp = true; }
            } else {
                //'if the selected card is 1 lower than the last card on the pile, and is opposite in color then
                if ( GetColor(PileList[14].CardList[0].Suite) != GetColor(PileList[SelectedPile].CardList[PileList[SelectedPile].CardCount - 1].Suite) ){
                    if (PileList[14].CardList[0].Value == PileList[SelectedPile].CardList[PileList[SelectedPile].CardCount - 1].Value - 1) { temp = true; }
                }
            }
		}
	}
	return temp;
}

int TopCard(){
    int temp, temp2;
	bool cont = true;
    temp2 = PileList[SelectedPile].CardCount - 1;
	temp = temp2 - 1;
    //for (temp = PileList[SelectedPile].CardCount - 2; temp >= 0; temp--) {
	while (cont) {
            //If the card is face down, or the same color as the one on top of it, or not 1 higher in value than the one on top of it then cancel
            if (!PileList[SelectedPile].CardList[temp].Face) { cont = false; }
            if (GetColor(PileList[SelectedPile].CardList[temp].Suite) == GetColor(PileList[SelectedPile].CardList[temp + 1].Suite) ) { cont = false; }
            if ( (PileList[SelectedPile].CardList[temp].Value - 1) != PileList[SelectedPile].CardList[temp + 1].Value ) { cont = false; }
            if (cont) {temp2 = temp;}
			temp = temp - 1;
			if (temp < 0) { cont = false; }
    }
    return temp2;
}

void RemoveCardFromStack(){
    if( PileList[14].CardCount > 0){
        PileList[14].CardList[0].Face = true;
        MoveCard ( PileList[14], 0, PileList[BufferPile]);
        if ( BufferPile == SelectedPile ) { SelectedCard = PileList[SelectedPile].CardCount - 1; }
    }
}

void SelectLastCard(){
    if (PileList[14].CardCount > 1) {
        while (PileList[14].CardCount > 1) {
            RemoveCardFromStack();
        }
    }
}

void PurgeStack(int destPile = -1){
    if (destPile == -1) { destPile = BufferPile; }
    while ( PileList[14].CardCount > 0 ) {
        PileList[14].CardList[0].Face = true;
        MoveCard (PileList[14], 0, PileList[destPile]);
    }
}


void SelectCards(){ //card piles
	int temp;//, temp2;
    if (PileList[14].CardCount == 0){// Then 'if there are no selected cards already,select as much as possible
        BufferPile = SelectedPile; //'Used to undo selection
        //'if the top card is face down, make it face up
        if (PileList[SelectedPile].CardCount > 0){
            if (!PileList[SelectedPile].CardList[PileList[SelectedPile].CardCount - 1].Face){
                UnflipACard();
                PileList[SelectedPile].CardList[PileList[SelectedPile].CardCount - 1].Face = true;
                //AUTOdrawscreen();
            } else {
                temp = SelectedCard;// TopCard();
				while ( PileList[SelectedPile].CardCount > temp) {
                //for (temp2 = temp; temp2 < PileList[SelectedPile].CardCount; temp2++ ){
                    MoveCard (PileList[SelectedPile], temp, PileList[14]);
                }
                //AUTOdrawscreen();
				SelectedCard = PileList[SelectedPile].CardCount-1 ;
            }
		}
    } else { //'place them down if you can, or are clicking the undo pile
        if (SelectedPile == BufferPile){
            PurgeStack();
			SelectedCard = PileList[SelectedPile].CardCount -1;
        } else {
            if (CanPlaceStack()) {
                SolitaireScore (BufferPile, SelectedPile);
                PurgeStack (SelectedPile);
				SelectedCard = PileList[SelectedPile].CardCount -1;
            }
        }
    }
}
void ForceWin(){
    static bool IsWinning = false;
	int temp;
    if (!IsWinning) {// Then Exit Sub 'prevents multiple instances as theyd interfere
		IsWinning = true;
		for (temp = 0; temp <= 14; temp++){
			EmptyPile (PileList[temp]);
		}
		AddCard (PileList[2], 14, 0);
		AddCard (PileList[3], 14, 1);
		AddCard (PileList[4], 14, 2);
		AddCard (PileList[5], 14, 3);
		for (temp = 2; temp < 14; temp++){
			AddCard (PileList[2], temp, 0);
			AddCard (PileList[3], temp, 1);
			AddCard (PileList[4], temp, 2);
			AddCard (PileList[5], temp, 3);
		}
		SelectedPile = -1;
		WINGAME();	
		IsWinning = false;
	}
}

void Undiscard(){
	if ( (PileList[14].CardCount == 0) && (PileList[13].CardCount > 0)) {
        MoveCard ( PileList[13], PileList[13].CardCount - 1, PileList[1] );
    }
}

void SelectCard(){//ACES
    if (PileList[14].CardCount == 0){// Then 'if there are no selected cards already, select one
        BufferPile = SelectedPile; //'Used to undo selection
        MoveCard (PileList[SelectedPile], PileList[SelectedPile].CardCount - 1, PileList[14] );
        if (SelectedCard >= PileList[SelectedPile].CardCount) { SelectedCard = SelectedCard - 1; }
        if ( (SelectedPile == 1) && (PileList[1].CardCount <= 1) ) {// 'SolitaireMoveCursor -1 'dealt cards is empty,
            Undiscard();
        }
        //AUTOdrawscreen();
    } else {// 'place them down if you can, or are clicking the undo pile
        if (SelectedPile == BufferPile){
            RemoveCardFromStack();
        } else {//
            if (CanPlaceStack()) {
                SolitaireScore (BufferPile, SelectedPile);
                PurgeStack (SelectedPile);
            }
        }
    }
    if (SelectedCard <0) { 
		SelectedCard = PileList[SelectedPile].CardCount - 1;
		//AUTOdrawscreen();
	}
}

void SolitaireAction(){
    if (SelectedCard == -1) { SelectedCard = 0; }
	switch (SelectedPile){
        case 0: //deck
			Deal3Cards (MaxCards); //deal 3 cards from the deck
		break;
        case 1: case 2: case 3: case 4: case 5://dealt cards and the ace pile
			SelectCard ();
		break;
        case 6: case 7: case 8: case 9: case 10: case 11: case 12:  //card piles
			SelectCards();
		break;
		default:
			
		break;
    }//selected cards pile cant be the selected pile so ignore
    if (HasWon()) { 
		WINGAME(); 	
	}
}

void AutoControl() {
//PSP_CTRL_SQUARE PSP_CTRL_SELECT PSP_CTRL_START PSP_CTRL_UP PSP_CTRL_RIGHT PSP_CTRL_DOWN 
//PSP_CTRL_RTRIGGER PSP_CTRL_TRIANGLE PSP_CTRL_CIRCLE PSP_CTRL_CROSS PSP_CTRL_LEFT PSP_CTRL_LTRIGGER 
	bool didsomething = false, doit = true, doitanyway = false;
	double timedifference = 0;
	char* value;
	int tempr;
	sceCtrlReadBufferPositive(&pad, 1);
	
	//If no cards are grabbed, and there are cards dealt, grab one. if cards are grabbed, put them back
	if ((!didsomething) && (pad.Buttons & PSP_CTRL_CIRCLE)) { didsomething = true;
		tempr = SelectedPile;
		SelectedPile = 1;
		/*
		if (PileList[14].CardCount == 0) {
			if (PileList[1].CardCount > 0) {
				SelectedPile = 1;
			}
		} else {
			SelectedPile = 1
		} */
		SolitaireAction();
		//SelectedPile = tempr;
		SelectedCard = PileList[SelectedPile].CardCount -1;
	}
	
	if ((!didsomething) && (pad.Buttons & PSP_CTRL_RIGHT)) { didsomething = true;
		SelectNextPile(12,0,1);
	}
	
	if ((!didsomething) && (pad.Buttons & PSP_CTRL_LEFT)) { didsomething = true;
		SelectNextPile(12,0,-1);
	}
	
	if ((!didsomething) && (pad.Buttons & PSP_CTRL_CROSS)) { didsomething = true;
		SolitaireAction();
	}
	
	//if holding R trigger too, start a new game. If not, deal cards from the deck
	if ((!didsomething) && (pad.Buttons & PSP_CTRL_LTRIGGER)) { didsomething = true;
		if (pad.Buttons & PSP_CTRL_RTRIGGER) {
			newgame();
		} else {
			Deal3Cards(MaxCards);
			if (SelectedPile == 1) { SelectedCard = PileList[1].CardCount - 1; }
		}
	}
	
	//if no cards are held, and selectedpile is a card pile, grab as many cards as you can. if cards are held, dump them to the ace piles
	if ((!didsomething) && (pad.Buttons & PSP_CTRL_RTRIGGER)) { didsomething = true;
		if (PileList[14].CardCount == 0) {
			if ((SelectedPile>5) && (SelectedPile<13)){
				SelectedCard = TopCard();
				SolitaireAction();
			} else {
				if (SelectedPile==1) {SelectCard();}
			}
		} else {
			DumpToAces();
			if (HasWon()) { 
				WINGAME(); 
			}
		}
	}
	
	//go to the card above selectedcard
	if ((!didsomething) && (pad.Buttons & PSP_CTRL_UP)) { didsomething = true;
		if ((SelectedPile > 5) && (SelectedPile < 13)) {
		
			int temp, temp2;
			temp = SelectedCard;
			temp2 = TopCard();
			if ( (SelectedCard > TopCard()) && (PileList[14].CardCount == 0) ) {
				SelectedCard = SelectedCard -1;
			} else {
				switch (SelectedPile){
					case 6: SelectedPile = 0; break;//left most card pile, move to deck
					case 7: case 8: SelectedPile = 1; break; //second and third left most card piles, move to dealt
					case 9: case 10: case 11: case 12: SelectedPile = SelectedPile - 7; break; //rest of the card piles, move to the ace pile above it
					default: doit = false; break;
				}
				if (doit) { SelectedCard = PileList[SelectedPile].CardCount-1 ; }
			}
		}		
	}
	
	//go to the card below selectedcard
	if ((!didsomething) && (pad.Buttons & PSP_CTRL_DOWN)) { didsomething = true;
		if ((SelectedPile > 5) && (SelectedPile < 13)) {
			if (SelectedCard < PileList[SelectedPile].CardCount -1) {
				SelectNextCard();
			} 
		} else {
			switch (SelectedPile){
				case 0: SelectedPile = 6; break;//deck pile, move to left most card pile
				case 1: SelectedPile = 7; break; //dealt pile, move to second left most card pile
				case 2: case 3: case 4: case 5: SelectedPile = SelectedPile + 7; break; //ace piles, move to the card pile below it
				default: break;
			}
			SelectedCard = PileList[SelectedPile].CardCount-1 ;
		}
	}
	
	if ((!didsomething) && (pad.Buttons & PSP_CTRL_START)) { didsomething = true;
		AUTOdrawscreen(false);  
		AUTOdrawscreen(false);  
		
		if ( MaxCards == 3) { Mainmenu.MenuList[0].OptionIndex = 0; } else { Mainmenu.MenuList[0].OptionIndex = 1; }
		Mainmenu.MenuList[1].OptionIndex = ScoringMethod;
		Mainmenu.MenuList[2].OptionIndex = 0;
		if (TimedGame) { Mainmenu.MenuList[2].OptionIndex = 1; }
		Askmenu(Mainmenu,  pad);
		switch (Mainmenu.SelectedItem) {
			case 4://newgame
				if (Mainmenu.MenuList[0].OptionIndex == 0) { MaxCards = 3; } else { MaxCards = 1; }
				ScoringMethod = Mainmenu.MenuList[1].OptionIndex;
				TimedGame = (Mainmenu.MenuList[2].OptionIndex == 1);
				newgame();
			//case 7:
				//Continue = false;
			//break;
			
			default: break;
		}
		dooldtime();
	}
	
	if ((!didsomething) && (pad.Buttons & PSP_CTRL_SELECT)) { didsomething = true;
		AskForFile(pad);
	}

	if ( TimedGame ) { 	
		temptime = oldtime;
		dooldtime();
		newtime = oldtime;
		oldtime = temptime;
		timedifference = newtime - oldtime;
				
		if (timedifference >= 1) { 
		//if (NewTime > OldTime) { //(((NewTime - OldTime)/tickRes) > 1 ){
			OneSecondPassed();
			doitanyway = true;
		} 
	}
	
	
	if (didsomething || doitanyway) { 
		AUTOdrawscreen(false);  	
		if (ScoringMethod < 2) {
			if (TimedGame) {
				//sprintf(value, "Score: %d oldTime: %f", Score,timedifference);// TimePassed);
				//sprintf(value, "Score: %d Time: %f", Score, timedifference); //TimePassed);
				sprintf(value, "Score: %d Time: %d", Score, TimePassed);
			} else {
				sprintf(value, "Score: %d", Score);
			}
			printTextScreen (480 - TextWidth(value) , 272 - 8, value, 0xFFFFFFFF);
		}
		
		flipScreen();
		
		if (didsomething) {
			doEvents();
			doEvents();
		} 
	} else { HandleRemote(); }
}	

int main() {
	sceCtrlSetSamplingCycle(0); 
	sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);

	char buffer[200];
	tickRes = sceRtcGetTickResolution();	
	InitAudio();
	pspDebugScreenInit();
	SetupCallbacks(); 
	
	sprintf(buffer, "back.png");
	srcBack = loadImage(buffer); 
	
	sprintf(buffer, "Cards1.png");
    srcLeft = loadImage(buffer); 
	
	sprintf(buffer, "Cards2.png");
    srcRight = loadImage(buffer); 
	
	if (!srcLeft || !srcRight) {
		pspDebugScreenInit();
		printf("Images failed to load! Please download the dependancies\n");
	} else { 
	
		initsolitaire();
		newgame();	
		
		while (Continue) {
			AutoControl();
		}

	}
	//sceKernelSleepThread(); 
	sceKernelExitGame(); 
    return 0;
}; 



