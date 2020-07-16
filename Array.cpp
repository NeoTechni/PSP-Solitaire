#include <math.h>

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>

#include <pspdebug.h> 
#include <png.h>
extern "C" {
#include "graphics.h" 
}
#define printf pspDebugScreenPrintf
#define MAXARGS 31

/* Functions needed: 25 total
AddCard			Adds a card to a card pile
AddPile			Adds a new card pile to the pile list
AUTOdrawdeck	Draws a card pile from the pile list at the XY coordinates and style it's set to, taking account for selected pile and card
AUTOdrawscreen	Draws all cards in the card list
CardCount		Returns the number of cards in a card pile that match your search parameters. -1 for value or face is a wildcard
DealCards		Moves a certain number of cards from srcpile to destpile
DealCardsMulti	Moves multiple cards from a card pile to multiple card piles in the pile list
DeleteCard		Deletes a card from a card pile
DeletePiles		Deletes all card piles from the pile list
DrawCardPile	Draws a card pile to X, Y using the style you pick
DrawCardStyle	Draws a card of any value and suite to X, Y using the style you pick
EmptyPile		Empties a card pile of it's cards
Face2Value		NOT NEEDED. Converts a string value (such as those returned by Value2Face) to a numerical value. But this code uses int's instead of strings to hold values anyway
FindCard		Searches for a specific card (-1 for face/suite is a wildcard), lets you start from anywhere in the card pile
GetColor		Gets the color of a suite (red = false, black = true)
HasCard			Checks if the card pile has a certain card (-1 for suite checks for any suit)
MoveCard		Moves a card from a card pile to another card pile
MoveCards		Moves multiple cards from the end of a card pile starting from index, to another card pile
PileHeight		Gets the width (height) of a card pile
PileWidth		Gets the width (pixels) of a card pile
RelativeCard	If direction = 1, it returns the next highest card value with a max of 14/Ace, if dir = -1, then returns the lower card. If you go below 2, it returns 14-2
RevealHand		Sets all cards in a card pile to face up (or down if you want)
ShuffleDeck		Deals 52*decks+jokers cards to a card pile in random order
Value2Face		Returns the string to printf for a certain card value (use %s in printf)
Value2Suite		Returns the ASCII character to printf for a certain suite (use %s in printf)
*/

//#include "ListAPI.cpp"

int Zero = 0;

const int DeckCount = 1;//Max number of usable decks
const int FullCardWidth = 41, FullCardHeight = 60;//don't forget to add one to the draw width/height
const int PartCardWidth = 11, PartCardHeight = 10;
const int PartBackSize = 4;

char *suits[5] = { "", "\003", "\004", "\005" , "\006" };// \003 = heart  \004 = diamond    \005 = club   \006 = spade
char *faces[15] = {"Deck", "Joker", "2", "3", "4", "5", "6", "7", "8", "9", "10", "J", "Q", "K", "A" };

enum iCardConstants{FullFront, LeftFront, TopFront, FullBack, TopBack, LeftBack, HandOnly, None, Slit} ;//Deck card styles
enum iStackConstants{Deck, Dealt, Aces, Cards, Horizontal, Invisible} ;						              //Deck draw styles

struct Card{
	int Value;
	int Suite;
	bool Face;
	int X;
	int Y;
	//bool Selected;//multiselect option in the future?
} ;
	
struct CardPile{
	Card CardList[DeckCount * 64];//Why 64 and not 52? base 2 numbers are more efficient apparently
	int CardCount;
	int X;
	int Y;
	iStackConstants Style;
};

	CardPile *PileList;//EMPTY ARRAY
	int PileCount = 0;
	int SelectedPile = -1;
	int SelectedCard = -1;
	int sCardCount = 0;
	
	SceCtrlData pad; 
	bool Continue = true;
	
	bool isAnimated = true;
	int Speed = 5;
	
	Image* srcLeft;
	Image* srcRight;
	Image* srcBack;
	

int intDiv(int Number, int ByWhat){
	int temp = 0;
	while (Number > ByWhat) {
		Number = Number - ByWhat;
		temp++;
	}
	return temp;
}
	
int PileWidth(CardPile Pile, int SelectedItem = -1, int Index = -1) {
	//Deck, Dealt, Aces, Cards, Horizontal 
	if (Index == -1) {Index = Pile.CardCount;}
	switch ( Pile.Style ){
		case Deck:// case Aces: case Cards: // [    ]
			return FullCardWidth + 1 + (intDiv(Index, 3)*2);
		break;
		
		case Horizontal:// [    ] [    ] [    ]
			return (Index - 1) * (FullCardWidth + 1);
		break;
		
		case Dealt:// [ [ [    ]
			if (SelectedItem > -1 && SelectedItem < Index - 1) {
                return (FullCardWidth + 1) + (Index - 2) * FullCardWidth;
            } else {
                return Index * FullCardWidth;
            }
		break;
		
		default:
			return FullCardWidth + 1;
		break;
	}
}

int PileHeight(CardPile Pile, int SelectedItem = -1, int Index = -1) {
	//Deck, Dealt, Aces, Cards, Horizontal 
	if (Index == -1) {Index = Pile.CardCount;}
	int Height = FullCardHeight + 1, temp;
	switch ( Pile.Style ){
		//case Deck: case Aces: case Horizontal: case Dealt: // [   ]
		//	return FullCardHeight + 1;
		//break;
				
		case Cards:// [ [ [   ]
			for (temp = 0; temp < Index - 1; temp ++) {
				if (Pile.CardList[temp].Face) {
					Height = Height + PartCardHeight;
				} else {
					Height = Height + PartBackSize;
				}
			}
			return Height;
		break;
		
		default:
			return FullCardHeight + 1;
		break;
	}
}


//Selects the next pile (or previous if direction = -1)
void SelectNextPile(int Max = -1, int Min = 0, int Direction = 1){
	if (Max == -1) { Max = PileCount - 1;}
	SelectedPile = SelectedPile + Direction;
	if (SelectedPile > Max) { SelectedPile = Min; }
	if (SelectedPile < Min) { SelectedPile = Max; }
	SelectedCard = PileList[SelectedPile].CardCount -1;
}

void SelectNextCard(int Direction = 1){
	SelectedCard = SelectedCard + Direction;
	if (SelectedCard >= PileList[SelectedPile].CardCount) { SelectedCard = 0; }
	if (SelectedCard < 0) { SelectedCard = PileList[SelectedPile].CardCount -1; }
}

void doEvents(){
	for(int i=0; i<5; i++) {
        sceDisplayWaitVblankStart();
    } 
}

//Delete all cardpiles
void DeletePiles() {
	PileCount = 0;
	sCardCount = 0;
}

//Create a new cardpile
int AddPile(int X, int Y, iStackConstants Style) {
	PileCount = PileCount + 1;
	PileList = (CardPile*) realloc( PileList , sizeof(CardPile) * PileCount); //REDIM PRESERVE
	PileList[PileCount-1].X = X;
	PileList[PileCount-1].Y = Y;
	PileList[PileCount-1].Style = Style;
	PileList[PileCount-1].CardCount=0;
	return PileCount-1;
}

//Add a Card to a cardpile
int AddCard(CardPile& Pile, int Value = 2, int Suite = 0, bool Face = false, int X =0, int Y = 0){
	//printf("%d : adding card \n", Value);
	//printf(" + ");
	Pile.CardCount = Pile.CardCount + 1;
	//printf("Added 1 to card count");
	
	Pile.CardList[Pile.CardCount - 1].Value = Value;
	Pile.CardList[Pile.CardCount - 1].Suite = Suite;
	Pile.CardList[Pile.CardCount - 1].Face = Face;
	Pile.CardList[Pile.CardCount - 1].X = X;
	Pile.CardList[Pile.CardCount - 1].Y = Y;
	//printf("set values");
		
	//Pile.CardList = (Card*) realloc( Pile.CardList, sizeof(Card) * Pile.CardCount);//REDIM PRESERVE
	//if ( Pile.CardList != NULL ) {
		//Pile.CardList[Pile.CardCount - 1].Selected = false;
	//}
	//printf("%d : added card \n", Value);
	//printf(" + ");
	sCardCount++;
	return Pile.CardCount-1;
}

//Remove a card from a cardpile
int DeleteCard(CardPile& Pile, int index) {
	//printf(" - ");
	if ( index >=0 && index < Pile.CardCount ) {
		//If the card to be removed, is not the last card, shift all cards after it down one slot 
		if ( index < Pile.CardCount-1) {
			for ( int temp = index ; temp < Pile.CardCount; temp++ ) {
				Pile.CardList[temp] = Pile.CardList[temp+1];
			}
		}
		Pile.CardCount = Pile.CardCount - 1;
		sCardCount--;
	}
	//printf(" - ");
	return Pile.CardCount;
}

//Add a specific card from srcpile to destpile, then remove it from srcpile
void MoveCard(CardPile& srcpile, int index, CardPile& destpile){
	int cX=0, cY=0, dX=0, dY=0, eX=0, eY=0;
	//printf("%d : moving card \n", index);
	//printf("( %d", srcpile.CardCount);
	if ( index >= 0 && index < srcpile.CardCount ){
		//CODE TO ANIMATE CARD MOVEMENTS!!!
		if (isAnimated) {
			cX = PileWidth(srcpile, index-1);
			cY = PileHeight(srcpile, index-1);
			dX = PileWidth(destpile, -1);
			dY = PileHeight(destpile, -1);
			eX = dX - cX;
			eY = dY - cY;
		}
		//CODE TO ANIMATE CARD MOVEMENTS!!!
		AddCard (destpile, srcpile.CardList[index].Value , srcpile.CardList[index].Suite, srcpile.CardList[index].Face, eX, eY);
		DeleteCard (srcpile, index);
	}
	//printf(" )");
}

//Moves cards from srcpile starting from index to the last card, to destpile
void MoveCards(CardPile& srcpile, int index, CardPile& destpile){
	int Count =  srcpile.CardCount - index;
	for ( int temp = index ; temp < Count ; temp++) {
		MoveCard (srcpile, index, destpile);
	}
}

//Moves a certain number of cards from srcpile to destpile
//I don't know why I didn't just make this call movecards srcpile.cardcount-cards-1
void DealCards(CardPile& srcpile, CardPile& destpile, int Cards = 1){
	int temp = 0;
	for (temp = 0 ; temp < Cards ; temp++) {
		MoveCard (srcpile, srcpile.CardCount - 1, destpile);
	}
}

//Flips all cards in a cardpile over
void RevealHand(CardPile& Pile, bool Face = true) {
	int temp = 0;
	for (temp = 0 ; temp < Pile.CardCount; temp++) {
		Pile.CardList[temp].Face = Face;
	}
}

//Converts a suite value to an output/display string
//don't forget to use %s instead of %d in printf!!
char* Value2Suite(int Index){
	return suits[Index];
}

//Converts a face value to an output/display string
//don't forget to use %s instead of %d in printf!!
char* Value2Face(int Index){
	return faces[Index];
}


//Printf's a single card
void PrintCardStyle(int X, int Y, iCardConstants Style, int Value = 2, int Suite = 1, bool Hand = false){
	switch (Style){
		case FullFront: // [#$  ]
			printf("[%d%s  ]", Value, Value2Suite(Suite) );
		break;
        case LeftFront: // [#$  
			printf("[%d%s", Value, Value2Suite(Suite) );
		break;
        case FullBack:  // [XXXX]
			printf("[XXXX]");
		break;
		//case TopFront:
        //case TopBack:
		default:

		break;
    }
}

//Printf's a bunch of cards
void PrintCardPile(CardPile Pile, int X, int Y, int SelectedItem = -1) {
	int temp;
	printf("%d cards: ", Pile.CardCount);
	for(temp = 0; temp < Pile.CardCount; temp++) {
		PrintCardStyle(X, Y, FullFront, Pile.CardList[temp].Value, Pile.CardList[temp].Suite, SelectedItem == temp);
	}
}

void Offset(int& Dest, int& Off){
	Dest = Dest + Off;
	if (Off > 0) {
		if (Off > Speed) {
			Off = Off - Speed;
		} else {
			Off = 0;
		}
	} else {
		if (Off < -Speed) {
			Off = Off + Speed;
		} else {
			Off = 0;
		}
	}
}

//Draws a single card, FullCardWidth = 41, FullCardHeight = 60, PartCardWidth = 11, PartCardHeight = 10, PartBackSize = 4;
bool DrawCardStyle(Image* Source, Image* Source2, int X, int Y, iCardConstants Style, int Value = 2, int Suite = 1, bool Hand = false, int& OffX = Zero, int& OffY = Zero){	
	bool temp = false;

	if (OffX != 0 || OffY != 0) {
		Offset (X, OffX);
		Offset (Y, OffY);
		temp = true;
		switch (Style){
			case TopFront: case LeftFront: 
				Style = FullFront;
			break;
			case TopBack: case LeftBack:
				Style = FullBack;
			break;
			default:
			
			break;
		}
	}
		
	switch (Style){
		//Card shown from the front
		case FullFront: // [#$  ]
			if (Value <= 7) {
				blitAlphaImageToScreen( FullCardWidth *Value, FullCardHeight*Suite, FullCardWidth+1, FullCardHeight+1, Source, X, Y);
			} else {
				blitAlphaImageToScreen( FullCardWidth *(Value-8)+1, FullCardHeight*Suite, FullCardWidth+1, FullCardHeight+1, Source2, X, Y);
			}
		break;
		
		case TopFront:
			if (Value <= 7) {
				blitAlphaImageToScreen(FullCardWidth*Value, FullCardHeight*Suite, FullCardWidth+1, PartCardHeight, Source, X, Y);
			} else {
				blitAlphaImageToScreen(FullCardWidth*(Value-8)+1, FullCardHeight*Suite, FullCardWidth+1, PartCardHeight, Source2, X, Y);
			}
		break;
		
        case LeftFront: // [#$  
			if (Value <= 7) {
				blitAlphaImageToScreen(FullCardWidth*Value, FullCardHeight*Suite, PartCardWidth, FullCardHeight+1, Source, X, Y);
			} else {
				blitAlphaImageToScreen(FullCardWidth*(Value-8)+1, FullCardHeight*Suite, PartCardWidth, FullCardHeight+1, Source2, X, Y);
			}
		break;
		
		//Card shown from the back
        case FullBack:  // [XXXX]
			blitAlphaImageToScreen(0, FullCardHeight, FullCardWidth+1, FullCardHeight+1, Source, X, Y);
		break;
		
        case TopBack:
			blitAlphaImageToScreen(0, FullCardHeight, FullCardWidth+1, PartBackSize, Source, X, Y);
		break;
		
		case LeftBack:
			blitAlphaImageToScreen(0, FullCardHeight, PartBackSize, FullCardHeight+1, Source, X, Y);
		break;
		
		case Slit:
			blitAlphaImageToScreen(0, FullCardHeight, 2, FullCardHeight+1, Source, X, Y);
		break;
		
		//New styles
		case None:
			blitAlphaImageToScreen(0, FullCardHeight*2, FullCardWidth+1, FullCardHeight+1, Source, X, Y);
		break;
		
		default:
		
		break;
    }
	if (Hand) { blitAlphaImageToScreen(0, 0, FullCardWidth, FullCardHeight, Source, X, Y); }
	return temp;
}


bool DrawCardPile(CardPile Pile, int X, int Y, int selecteditem = -1){
	bool temp2 = false;
	
	if ((selecteditem > Pile.CardCount -1) && (Pile.CardCount > 0) ) { selecteditem = Pile.CardCount -1; }
	int temp = 0;//Note: Styles are based off Windows standard solitaire
	if ( (Pile.CardCount == 0)  && (Pile.Style != Horizontal) && (Pile.Style != Dealt) ) { //&& (Pile.Style != Aces) && (Pile.Style != deck)  ) { // NO CARDS IN DECK, DRAW EMPTY DECK PLACEHOLDER
			temp2 = temp2 || DrawCardStyle ( srcLeft, srcRight, X, Y, None, 0,0, (selecteditem > -1) );
		//} //Empty selected deck
    } else {
		switch (Pile.Style){
			case Deck://The deck in the upper left hand corner, draw the back of the deck
				for (temp = 0; temp < intDiv(Pile.CardCount, 3); temp++ ) {
					temp2 = temp2 || DrawCardStyle ( srcLeft, srcRight, X + (temp*2) , Y, Slit, 0, 0, false);
				}
				temp2 = temp2 || DrawCardStyle ( srcLeft, srcRight, X  + intDiv(Pile.CardCount, 3)*2 , Y, FullBack, 0, 0, (selecteditem > -1) );
			break;
			case Aces://The aces piles in the top right hand side, draw the front of the last card
			    temp2 = temp2 || DrawCardStyle(srcLeft, srcRight, X, Y, FullFront, Pile.CardList[Pile.CardCount - 1].Value, Pile.CardList[Pile.CardCount - 1].Suite, (selecteditem > -1) );	
			break;
			case Dealt://The dealt cards pile to the right of the deck, draw the top bit of all cards except for the selected one, and the last one.
				if ( (Pile.CardCount == 0) && (selecteditem > -1) ) {
					temp2 = temp2 || DrawCardStyle (srcLeft, srcRight, X, Y, HandOnly, 0,0,true);
				} else {
					for (temp = 0; temp < Pile.CardCount; temp++ ) {
						if ( (selecteditem == temp) || (temp == Pile.CardCount - 1) ) {
							temp2 = temp2 || DrawCardStyle(srcLeft, srcRight, X, Y, FullFront, Pile.CardList[temp].Value, Pile.CardList[temp].Suite, (selecteditem == temp) );	
							X = X + FullCardWidth; //shift right the full width of the card
						} else {
							temp2 = temp2 || DrawCardStyle(srcLeft, srcRight, X, Y, LeftFront, Pile.CardList[temp].Value, Pile.CardList[temp].Suite, false);	
							X = X + PartCardWidth; //shift right the part width of the card
						}
					}
				}
			break;
			case Cards://The 7 piles of cards on the bottom half of the screen
				if ( (Pile.CardCount == 0) && (selecteditem > -1) ) {
					temp2 = temp2 || DrawCardStyle (srcLeft, srcRight, X, Y, HandOnly, 0,0,true);
				} else {
					for (temp = 0; temp < Pile.CardCount; temp++ ) {
						if ((temp == Pile.CardCount - 1) || (selecteditem == temp)) {// Then 'Last card in pile
							if (Pile.CardList[temp].Face) { // 'Is uncovered, draw the front of the card
								temp2 = temp2 || DrawCardStyle ( srcLeft, srcRight, X, Y, FullFront, Pile.CardList[temp].Value, Pile.CardList[temp].Suite, (selecteditem == temp)  );
								Y = Y + FullCardHeight;
							} else { //is not uncovered, draw the back of the card
								temp2 = temp2 || DrawCardStyle ( srcLeft, srcRight, X, Y, FullBack, 0, 0, (selecteditem > -1) );
							}//Y doesnt need to be incremented since no cards are being drawn after this
						} else { // 'Is not the last card
							if (Pile.CardList[temp].Face) { // 'Is uncovered, draw the top bit of the front of the card
								temp2 = temp2 || DrawCardStyle ( srcLeft, srcRight, X, Y, FullFront, Pile.CardList[temp].Value, Pile.CardList[temp].Suite, -1);// 'cannot be selected
								Y = Y + PartCardHeight;// 'shift down the height of the card
							} else { //is not uncovered, draw the top bit of the back of the card
								temp2 = temp2 || DrawCardStyle ( srcLeft, srcRight, X, Y, TopBack, 0,0,-1);
								Y = Y + PartBackSize;// 'shift down the height of the card
							}
                        }
                    }
				}
				
			break;
			case Horizontal://All cards laid out fully horrizontally oriented
				for (temp = 0; temp < Pile.CardCount; temp++ ) {
					if (Pile.CardList[temp].Face) { // 'Is uncovered, draw the front of the card
                        temp2 = temp2 || DrawCardStyle(srcLeft, srcRight, X, Y, FullFront, Pile.CardList[temp].Value, Pile.CardList[temp].Suite, (selecteditem == temp) );
                    } else { //is not uncovered, draw the back of the card
                        temp2 = temp2 || DrawCardStyle ( srcLeft, srcRight, X, Y, FullBack, 0, 0, (selecteditem == temp) );
                    }
                    X = X + FullCardWidth+ 2; //shift right the full width of the card + 2 pixels of whitespace
				}
			break;
			default://Unknown or no deck present. Just draw the hand
				temp2 = temp2 || DrawCardStyle ( srcLeft, srcRight, X, Y, HandOnly, 0,0,true);
			break;
		}
	}//HandOnly, srcLeft, srcRight
	return temp2;
}

//Draw a deck
bool AUTOdrawdeck(int pileindex){
	int SelectedItem = -1;
	if (SelectedPile == pileindex) { SelectedItem = SelectedCard; }
	//if ( ( (PileList[pileindex].Style == Dealt) || (PileList[pileindex].Style == Aces) || (PileList[pileindex].Style == Cards) || (PileList[pileindex].Style == Deck) ) &&
	if ( (SelectedPile == pileindex) && (PileList[pileindex].CardCount == 0) ) { SelectedItem = 0; }
	if (PileList[pileindex].Style != Invisible) {
		return DrawCardPile ( PileList[pileindex], PileList[pileindex].X, PileList[pileindex].Y, SelectedItem );
	} else { return false; }
}

//Draws all decks
void AUTOdrawscreen(bool clear = true){
	int x = 0, y = 0, temp;
	bool temp2;
	sceDisplayWaitVblankStart();

	temp2 = true;
	
	while (temp2) {
		temp2 = false;
		
		if (!srcBack) {
			if (clear) { clearScreen(0x00); }
		} else {
			while (x < 480) {
				while (y < 272) { 
					blitAlphaImageToScreen(0, 0, 64, 64, srcBack, x, y);
					y += 64; //srcBack.imageHeight;
				} 
				x += 64; //srcBack.imageWidth;
				y = 0;
			} 
		}
	
		for (temp = 0; temp < PileCount; temp++) {	
			temp2 = temp2 || AUTOdrawdeck (temp);
		}
		if (clear && !temp2) { flipScreen(); }
	}
	
	//printTextScreen (0,0, , 0xFFFFFFFF);
	
}

//PARAMETER ARRAY
void DealCardsMulti(CardPile& srcPile, int Cards, int *destPiles, ... ){
	int temp, arg; //arg will be used to store the contents of the parameter array 
	va_list argptr; //declare a variable "argptr" of "va_list"
	for (temp = 0; temp < Cards; temp ++) {
		va_start(argptr, destPiles);//Go to first parameter of destPiles
		while( destPiles != 0 ) {
			arg = va_arg(argptr, int);//Get next parameter
			MoveCard(srcPile, srcPile.CardCount - 1, PileList[arg]);
		}
		va_end (argptr);//close the paramarray
	}
}

//// 0 \003 = heart  1 \004 = diamond   2 \005 = club  3 \006 = spade
bool GetColor(int Suite) {
    return Suite > 1;
}

int CardCount(CardPile Pile, int Value = -1, int Suite = -1) {
    int temp = 0, temp2 = 0;
    if (Pile.CardCount > 0) {
        for (temp = 0; temp < Pile.CardCount; temp++ ){
            if (Value > -1) {// Then 'has a value
                if (Suite > -1 ) {// Then 'has a suite too
					if ( (Pile.CardList[temp].Value == Value) && (Pile.CardList[temp].Suite == Suite) ) { temp2 = temp2 + 1; }
                } else {// 'use any suite
                    if ( Pile.CardList[temp].Value == Value ) { temp2 = temp2 + 1; }
                }
            } else { //'has only a suite
                if ( Pile.CardList[temp].Suite == Suite ) { temp2 = temp2 + 1; }
            }
        }
    }
    return temp2;
}

int FindCard(CardPile Pile, int Value = -1, int Suite = -1, int Start = 0) {
    int temp = 0, temp2 = -1;
    if (Pile.CardCount > 0) {
        for (temp = Start; temp < Pile.CardCount; temp++ ){
            if (Value > -1) {// Then 'has a value
                if (Suite > -1 ) {// Then 'has a suite too
					if ( (Pile.CardList[temp].Value == Value) && (Pile.CardList[temp].Suite == Suite) ) { temp2 = temp; }
                } else {// 'use any suite
                    if ( Pile.CardList[temp].Value == Value ) { temp2 = temp; }
                }
            } else { //'has only a suite
                if ( Pile.CardList[temp].Suite == Suite ) { temp2 = temp; }
            }
        }
    }
    return temp2;
}

int RelativeCard(int Value, int Direction = 1) {
    Value = Value + Direction;
    if (Value > 14) { Value = 14; }
    if (Value < 2) { Value = (Value % 14) + 13; }
    return Value;
}

bool HasCard(CardPile Pile, int Value, int Suite = -1) {
    return CardCount(Pile, Value, Suite) > 0;
}

void EmptyPile(CardPile& Pile){
    Pile.CardCount = 0;
}

void EmptyList(){
	int temp = 0;
	for (temp = 0; temp < PileCount; temp++) {	
		EmptyPile (PileList[temp]);
	}
}

int Face2Value(char* Text) {
	int temp, temp2 = 0;
	for (temp = 0; temp < 15; temp++) {
		if (strcmp(Text, faces[temp]) == 0) {
			temp2 = temp;
		}
	}
	return temp2;
}


void SeedRandom(){
	srand(time(NULL)); 
}

int RandomNumber(int Max, int Min = 0){
	return (rand()%(Max-Min) ) + Min;
};

void ShuffleDeck(CardPile& Pile, int Decks = 1, int Jokers = 0){
	if ( Decks > DeckCount ) { Decks = DeckCount; }
	if ( Jokers > DeckCount * 12 ) { Jokers = DeckCount * 12; }//I left room for 64 cards per deck instead of 52, which leaves 12 slots for jokers per deck allocated
	SeedRandom();
	CardPile srcpile;
	EmptyPile (srcpile);
	
	//printf("Beginning shuffle with %d cards \n", srcpile.CardCount);
	int temp, temp2, temp3 = 0;//, cards = 0;
	//printf("Adding jokers \n");
	for (temp2 = 0; temp2 < Jokers; temp2++) {
		AddCard (srcpile, 1, temp3, true);
		//cards++;
		temp3++;
		if (temp3 == 4) { temp3 = 0; }
	}
	//printf("Jokers added: %d \n", srcpile.CardCount);
	for (temp = 0; temp < Decks; temp++) {
		//printf("Adding deck %d \n Adding value: ", temp);
		for (temp2 = 2;  temp2 < 15; temp2++) {
			//printf("%d, ", temp2);
            AddCard (srcpile, temp2, 0, true);
            AddCard (srcpile, temp2, 1, true);
            AddCard (srcpile, temp2, 2, true);
            AddCard (srcpile, temp2, 3, true);
			//cards = cards + 4;
        }
    }
	
	//printf("\n Before: %d cards in the srcdeck \n", srcpile.CardCount);
	while (srcpile.CardCount > 1) {
		MoveCard ( srcpile, RandomNumber(srcpile.CardCount - 1), Pile );
		//cards = cards - 1;
		//printf("\n");
	}
	AddCard( Pile, srcpile.CardList[0].Value , srcpile.CardList[0].Suite, true);
	
	//printf("After: %d cards in the srcdeck \n", srcpile.CardCount);
}

