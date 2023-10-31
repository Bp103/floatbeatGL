#include <math.h>
#include <stdio.h>
#include <GL/freeglut.h>
#include <portaudio.h>

#ifndef M_PI
	#define M_PI 3.14159265359
#endif
#ifndef true
	#define true 1
#endif
#ifndef false
	#define false 0
#endif

#define SAMPLE_RATE 44100
#define AMPLITUDE   0.15

#define winX glutGet(GLUT_WINDOW_WIDTH)
#define winY glutGet(GLUT_WINDOW_HEIGHT)

//redefine printf as a comment.. because lazy.
//#define printf //

#define GRAVITY 0.1

PaError err;
PaStream* stream;

struct mouse {
	long x,y;
	int click;
	int left;
	int mid;
	int right;
}; struct mouse mouse;

#define maxObjects 2000
#define defObjects 9
int numObjects = defObjects;

struct rgbf {
	float r, g, b;
};
struct vect3 {
	double x, y, z;
};

struct object {
	float x,y;
	int state;
	float size;
	struct vect3 velocity;
	struct rgbf color;
	double damping;
}; struct object object[maxObjects];

float square[10][2]={
	{0,0},
	{0,1},
	{0,1},
	{1,1},
	{1,1},
	{1,0},
	{1,0},
	{0,0},
};

unsigned long t = 0;
unsigned long soundBuffer = 0;
double ticks, f[100],drum[100],retrigDrum[100],retrigSynth[100],noise[1000],volume=1;
int play=0,keyUp[256];
int mouse_x, mouse_y, xx, yy, increments, isound, movingWindow = false;
float sndBuff[SAMPLE_RATE], scopeBuff[20][SAMPLE_RATE];

// --> utility functions
double Mod(double dividend, double divisor){
	if (divisor == 0) {return 0;}
	double quotient = dividend / divisor;
	double wholePart = (int)quotient;
	return dividend - (wholePart * divisor);
}

double lerp(double a, double b, double speed){
	double lout = a + speed * (b - a);
	return (lout < 0)?0:lout;		// clamps at 0, will not go into negitive values.
}

double lerpc(double a, double b, double speed){
	double lout = a + speed * (b - a);
	return (lout < (a/2))?0:lout;		// once it reaches a/2 it warps to 0.
}

double lerps(double la, double lb, double lspeed){
	double lout = la + lspeed * (lb - la);
	return lout;		// Not clampped at all
}

float fRnd(float min, float max) {
	return min + (float)rand() / RAND_MAX * (max - min);
}

float razorDistortion(float input,float distortion,float wet,float dry){
	return (input*dry)-(((input > distortion)+(input < 0-distortion))*wet);
}
// <-- utility functions

int len(char *textInput){
	//get text/buffer length the hard way.
	int output=0;
	for(int i=0;textInput[i]!='\0';i++)output=i+1;
	return output;
}
void printg(int x, int y, char *text){
	//graphical print.
	int textLen=0;
	// string limit 1024 char
	for(int i=0;((text[i]!='\0')|(i>1024));i++)textLen=i+1;
	for(int i=0;i<textLen;i++)
	{
		int textX=x+(i*8);
		if((winX<textX)||(textX>0))
		{
			glRasterPos2i(textX,y);
			glutBitmapCharacter(GLUT_BITMAP_8_BY_13, text[textLen+(i-textLen)]);
		}else{
			break;
		}
	}
}

void scopeHere(int channel,double x, double y, double size ){
	int xold=x, yold=(y+size/2)+(scopeBuff[channel][0]*size);
	for(int i=0;i<soundBuffer; i++){
		glVertex2f(xold,yold);
		float sx = x+(i*(size/soundBuffer)), sy = (y+size/2)+(scopeBuff[channel][i]*size);
		glVertex2f(sx,sy);
		yold = sy; xold = sx;
	}
}

void setupObjects(){
	for(int i=0;i<maxObjects;i++)
	{
		if(i<defObjects){
			object[i].x= 4+(i*104);
			object[i].size= 101;
		}else{
			object[i].x= 10+fRnd(20,winX-100);
			object[i].size= fRnd(20,100);
		}
		object[i].y= (winY*2)+fRnd(20,winY*2);
		object[i].color.r = fRnd(0.5,1.0);
		object[i].color.g = fRnd(0.5,1.0);
		object[i].color.b = fRnd(0.5,1.0);
		object[i].damping = fRnd(0.1,0.95);
	}
}

float frame1,frame2,fps, ffps, ifps, targetSpeed;
int timer, realTime, realTimesec;

void display()
{

	//fps based timer setup
	if(!movingWindow)frame2=glutGet(GLUT_ELAPSED_TIME);
	fps=(frame2-frame1);
	frame1=glutGet(GLUT_ELAPSED_TIME);

	//fps counter
	realTime=glutGet(GLUT_ELAPSED_TIME);
	if(realTimesec<realTime)
	{
		ffps=ifps;
		ifps=0;
		realTimesec=realTime+1000;
	}
	ifps++;
	targetSpeed = (int)(fps/0.80f);
	if(movingWindow)targetSpeed=2;

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// --> Clickable object
	for (int i = 0; i < numObjects; i++) {

		object[i].velocity.y += GRAVITY * targetSpeed;
		object[i].y -= object[i].velocity.y * targetSpeed;

		if (object[i].y < 0) {
			object[i].y = 0;
			object[i].velocity.y = -object[i].velocity.y * object[i].damping; // Apply damping
		}

		// Handle object interaction
		if (mouse.click && (mouse.x > object[i].x && mouse.x < object[i].x + object[i].size) &&
			(mouse.y > object[i].y && mouse.y < object[i].y + object[i].size)) {
			object[i].x = mouse.x - (object[i].size / 2);
			object[i].y = mouse.y - (object[i].size / 2);
			object[i].velocity.y = 0;
		}

		for (int j = 0; j < numObjects; j++) {
			if (i != j) {

				if (object[i].x + object[i].size > object[j].x && object[i].x < object[j].x + object[j].size &&
					object[i].y + object[i].size > object[j].y && object[i].y < object[j].y + object[j].size) {
					// Collision detected ^^

					if (object[i].y + object[i].size > object[j].y) { // handle the collision
						object[i].y = object[j].y + object[j].size;				// Adjust position
						object[i].velocity.y = -object[i].velocity.y * object[i].damping/1.2;	// Apply damping to primary object
						object[j].velocity.y = -object[j].velocity.y * object[j].damping/10;	// Apply even harder damping to secondary object (fixes forever bounce)
					} else {
						object[i].velocity.y = -object[i].velocity.y * object[i].damping*fps;	// Apply damping like normal
					}
				}
			}
		}

		if(i<defObjects){
			glBegin(GL_LINES);
		}else{
			glBegin(GL_POLYGON);
		}
		glColor3f(object[i].color.r, object[i].color.g, object[i].color.b);
		for(int j=0;j<8;j++){
			if(object[i].y>winY)break;
			glVertex2f((object[i].size-1)*square[j][0]+object[i].x,(object[i].size-1)*square[j][1]+object[i].y);
		}
		if(i<defObjects)scopeHere(i,object[i].x,object[i].y,object[i].size-1);
		glEnd();
	}
	// <-- Clickable object

	// --> Scope view
	glColor3f(1,1,1);
	glBegin(GL_LINES);

	int xold=0, yold=(winY/2)+sndBuff[0]*(winY/4);
	for(int i=0;i<soundBuffer; i++){
		glVertex2f(xold,yold);
		float sx = i*(winX/(soundBuffer/1.0005)), sy = (winY/2) + (sndBuff[i]*volume)*(winY/4);
		glVertex2f(sx,sy);
		yold = sy; xold = sx;
	}

	glFlush();
	glEnd();
	// <-- scope view

	// --> Draw the text stuff
	glColor3f(1,1,1);
	char tbuffer[50];
	int n=sprintf(tbuffer, "Press escape to exit.");
	printg(10,winY-10,tbuffer);

	n=sprintf(tbuffer, "X:%i, Y:%i",winX,winY);
	printg(10,winY-25,tbuffer);

	n=sprintf(tbuffer, "t=%i",t);
	printg(10,winY-40,tbuffer);

	n=sprintf(tbuffer, "[Space] %s | volume: %i",(play?"Pause":"Play"),(int)(volume*100));
	printg(10,winY-55,tbuffer);

	n=sprintf(tbuffer, "[+ or - ] Total Objects %i",numObjects);
	printg(10,winY-70,tbuffer);

	n=sprintf(tbuffer, "FPS:%.0f", ffps);
	printg(winX/2,winY-10,tbuffer);

	for(int i=0;i<numObjects;i++){
		if(i<defObjects){
			glColor3f(1,1,1);
		}else{
			glColor3f(0,0,0);
		}
		if(object[i].size>20){
			n=sprintf(tbuffer, "%i",i);
			printg(object[i].x,object[i].y+object[i].size/2,tbuffer);
		}
	}
	// <-- text stuff

	// --> Mouse
	glColor3f(mouse.left*1,mouse.mid*1,mouse.right*1);
	glBegin(GL_POLYGON);

	glVertex2f(mouse.x,mouse.y);
	glVertex2f(mouse.x+15,mouse.y-15+mouse.click);
	glVertex2f(mouse.x+5-(mouse.click/2),mouse.y-20);

	glFlush();
	glEnd();
	// <-- mouse

	glutSwapBuffers();
	glutPostRedisplay();

	movingWindow = false;
}

void mouseRoutine(int x, int y){
	mouse.x = x; // place current mouse pos in mouse_x
	mouse.y = winY-y;

	glutSetCursor(GLUT_CURSOR_NONE); // hide cursor
	glutPostRedisplay();
}

void ButtonMouse(int button, int state){
	char *buttons[5]={"left","middle","right","scroll forth","scroll back"};
	printf("[id:%i] %s mouse button %s\n",button,buttons[(button<=4)?button:5],(state==0)?"down":"up");
	mouse.click=(state==0)*10;

	switch(button){
		case 0:
			mouse.left = state;
		break;
		case 1:
			mouse.mid = state;
		break;
		case 2:
			mouse.right = state;
		break;
		case 3:
			volume += 0.01;
		break;
		case 4:
			volume -= 0.01;
			if(volume<0)volume=0;
		default:
		break;
	}
}

int terminate(){
	PaError err;
	printf("Pa_StopStream:");
	err = Pa_StopStream(stream);
	if (err != paNoError) {
		fprintf(stderr, "PortAudio error: %s\n", Pa_GetErrorText(err));
		return 1;
	}else{printf(" success\n");}

	printf("Pa_CloseStream:");
	err = Pa_CloseStream(stream);
	if (err != paNoError) {
		fprintf(stderr, "PortAudio error: %s\n", Pa_GetErrorText(err));
		return 1;
	}else{printf(" success\n");}

	printf("Pa_Terminate\n");
	Pa_Terminate();
	printf("GL_Terminate\n");
	exit(0);
}

void ButtonDown(unsigned char key){ //keyboard button pressed down
	if(keyUp[key]!=1){
		if(key>32){
			printf("[id:%i] Down \'%c\'\n",key,key);
		}else{
			printf("[id:%i] Down\n",key); // ascii < 32 here be dragons.
		}
	}

	int ucase=(key>=97 && key<=122)?32:0; // Convert to Capital (by subtracting 32 from key in lcase range)

	switch((key-ucase) * (keyUp[key]!=1)){
		case 27:
			printf("Escape pressed, exiting...\n");
			terminate();
		break;

		case 32:
			printf("[space] action: %s\n",(play?"play":"pause"));
			play = !play;
		break;

		case 'T':
			printf("\'t\' reset to 0.\n");
			t=0;
		break;
		case '+':
			numObjects++;
			if(numObjects>maxObjects)numObjects=maxObjects;
		break;
		case '-':
			numObjects--;
			if(numObjects<0)numObjects=0;
		break;

		case 'Y':
			setupObjects();
		break;
		case 'R':
			numObjects = defObjects;
		break;

		default:	//do nothing.
		break;
	}

	keyUp[key]=1; // Must be last in this function!
}

void ButtonUp(unsigned char key){ //keyboard button pressed down
	if(key>32){
		printf("[id:%i] Up   \'%c\'\n",key,key);
	}else{
		printf("[id:%i] Up\n",key);
	}
	keyUp[key]=0;
}

int arraySize(void* inputArray){
	return sizeof(inputArray) / sizeof(inputArray[0]);
}

double drumSynth(unsigned long t, float bpm,float pitch){
	float secondsPerBeat = 60.0 / (bpm*8);

	int sr = SAMPLE_RATE;
	double q= (2 * M_PI * t / SAMPLE_RATE);
	double drmOut=0;

	//Must use drum off command, its like midi.
	double _ = 0;	//drum off
	double X = 1;	//drum normal
	double L = 0.8;	//drum low
	double H = 1.3;	//drum high

	double seq[5][64]= {
			{L,_,_,_, _,_,_,_, _,_,_,_ ,L,_,_,_,  _,_,_,_, _,_,_,_, L,_,_,_ ,L,_,_,_,	//Kick
			 L,_,_,_, L,_,_,_, L,_,_,_ ,_,_,_,_,  _,_,_,_, L,_,_,_, _,_,_,_ ,H,_,X,_},

			{_,_,_,_, _,_,_,_, _,_,_,_ ,_,_,_,_,  H,H,X,X, _,_,_,_, _,_,_,_ ,_,_,_,_,	//Snare
			 _,_,_,_, _,_,_,_, _,_,_,_ ,_,_,_,_,  H,H,X,X, _,_,_,_, _,_,_,_ ,_,_,_,_},

			{X,_,_,_, X,_,_,_, X,_,_,_ ,X,_,_,_,  X,_,_,_, X,_,X,_, X,_,X,_ ,X,_,X,_,	//Hat
			 X,_,_,_, X,_,_,_, X,_,X,_ ,X,_,_,_,  X,_,X,_, X,_,_,_, X,_,_,_ ,X,_,X,_},

			{_,_,X,_, _,_,X,_, _,_,X,_ ,_,_,X,_,  _,_,X,_, _,_,X,_, _,_,X,_ ,_,_,X,_,	//Hi-Hat
			 _,_,X,_, _,_,X,_, _,_,X,_ ,_,_,X,_,  X,_,X,_, X,_,X,_, _,_,X,_ ,X,_,X,_}};

	int seqLenght = 64;
	int secCal = (int)(t / (SAMPLE_RATE * secondsPerBeat))%64; //(int)(q*secondsPerBeat)%seqLenght; //calculate place in sequence at bpm
	for(int i=0; i<4; i++){
	drum[i]=0; // set drum to 0 to stop rouge values.
	scopeBuff[5+i][isound] = drum[i];
	if(seq[i][secCal]==0)retrigDrum[i]=1;//Note cut detection

		if((seq[i][secCal]!=0) && (retrigDrum[i]==0)){
			f[i]++;	// per-channel virtual timer

			double qf=((pitch*seq[i][secCal])/8)*(2 * M_PI * f[i] / SAMPLE_RATE);
						// virtual quantizeation and crude pitch control. "qf=f[i]/sr"
			double fn=0;
			double fnb=0;
			switch(i){
				case 0:	// kick
					fn = lerpc(1500,0,qf*10);
					fnb = lerp(1.5,0,qf*24);
					drum[i] = sin(qf*fn)*fnb;
				break;
				case 1:	// snare
					fn = lerpc(4000,0,qf*2);
					fnb = lerp(1500,0,qf*7);
					drum[i] = (noise[(int)(Mod(qf*5e3,300))]*(qf>.02)*(fnb/3500)-sin(qf*1500)*(qf<.06)*(fn/5500))*1.5;
				break;
				case 2:	// hat
					fn = lerp(.3,0,qf*30);
					drum[i] = noise[(int)Mod(qf*3e4,1000)]*fn;
				break;
				case 3:	// Hi-hat
					fn = lerp(0.2,0,qf*64);
					drum[i] = ((noise[(int)Mod(qf*8e3,1000)]-(cos(qf*7e4)/3))*fn);
				break;
				default:
					drum[i]=0;
				break;
			}
			if(!drum[i])drum[i]=0;		// null protection
			if(fn<=0) retrigDrum[i]=1;	// retrigger set
			}else if((seq[i][secCal]==0) && (retrigDrum[i]==1)){
				retrigDrum[i]=0;
				f[i]=0;
			}

			if(i<2){
				scopeBuff[5+i][isound] = drum[i]/4;
			}else{
				scopeBuff[5+i][isound] = drum[i];
			}

			drmOut += drum[i];
		}
	return drmOut;
}

float synth(unsigned long t,float bpm, int transpose){
	// -- Basic settings
	float secondsPerBeat = 60.0 / (bpm*2);
	int currentNote = 0;  // Initialize the current note index
	// --

	int lnote[] = {58,00,61,63,00,66,66,68,00,00,70,71,00,68,00,70,00,73,78,76,76,75,00,80,83,00,70,00,83,66,68,00,
			68,70,71,00,70,68,00,73,71,00,68,00,66,67,68,00,73,73,75,00,66,68,00,66,68,00,71,73,75,68,66,68};
	int cnote[8][8] = {{70,66,54,73},{56,63,68,71},{68,71,75,66},{59,64,71,73},
				{54,66,70,73},{56,63,68,71},{71,64,56,68},{59,63,71,75}};
	int bnote[] = {44,44,44,44,00,56,44,44,56,56,63,63,61,61,56,56,40,40,40,40,00,56,40,44,52,52,63,63,59,59,51,51};

	float snd = 0;
	float q = 2 * M_PI * t / SAMPLE_RATE;
	float noteDur;
	// Calculate the current note index based on the current time
	currentNote = t / (SAMPLE_RATE * secondsPerBeat);

	noteDur = t/(SAMPLE_RATE*secondsPerBeat);
	int np = (int)(noteDur/4)%8;
	double fade = (int)Mod((noteDur*(bpm/256)),2)+1;	// chord
	for(int ni=0;ni<4;ni++){
			float freq = q * 55 * pow(pow(2, 1 / 12.0), cnote[np][ni]-24+transpose);
			float chord = AMPLITUDE * sin(sin((freq/2)+sin(freq*(1.001+(ni*0.003))))) * lerp(0.7,.00,Mod(noteDur/fade,1));
			scopeBuff[0][isound] = chord;
			snd += chord;
	}

	noteDur = t/(SAMPLE_RATE*secondsPerBeat);	// arpA
	np = (int)(noteDur/4)%8;
	int nj = (int)(noteDur*2)%3;
	if(cnote[np][nj]!=0){
			float freq = q * 55 * pow(pow(2, 1 / 12.0), cnote[np][nj]-24+transpose);
			float arpA = AMPLITUDE * sin((sin(freq)>cos(noteDur*8)*0.99)/sin((freq/2)+sin(freq*3))) * lerp(0.3,0.05,Mod(noteDur*2,1));
			scopeBuff[1][isound] = arpA;
			snd += arpA;
	}

	noteDur = t/(SAMPLE_RATE*secondsPerBeat);	// arpB
	np = (int)(noteDur/4)%8;
	nj = (int)(noteDur*4)%3;
	if(cnote[np][nj]!=0){
			float freq = q * 55 * pow(pow(2, 1 / 12.0), cnote[np][2-nj]-24+transpose);
			float arpB = AMPLITUDE * sin((sin(freq)<cos(noteDur*8)*0.99)/sin((freq*2)+sin(freq*2.5))) * lerp(0.2,0.05,Mod(noteDur*4,1));
			scopeBuff[2][isound] = arpB;
			snd += arpB;
	}

	if(bnote[currentNote%32]!=0){
		float freq = q * 55 * pow(pow(2, 1 / 12.0), bnote[currentNote%32]-36+transpose);	// bass
		noteDur = t/(SAMPLE_RATE*secondsPerBeat);
		float bass = sin((freq - sin(freq/2.03)) + (sin(freq*4)*0.3))/3.5;
		float dist=0.15;
		bass = razorDistortion(bass,dist,0.08,1);
		scopeBuff[3][isound] = bass;
		snd += bass;
	}else{
		scopeBuff[3][isound] = 0;
		snd += 0;
	}

	if(lnote[currentNote,0]!=0){
		float freq = q * 55 * pow(pow(2, 1 / 12.0), lnote[(currentNote%32)+(((int)(noteDur)%128)<64?0:32)]-36+transpose);	// lead
		noteDur = t/(SAMPLE_RATE*secondsPerBeat);
		float lead = AMPLITUDE * sin(sin(freq)-cos(noteDur*8)) * lerp(2.5,.00,Mod(noteDur,1));
		scopeBuff[4][isound] = lead;
		snd += lead;
	}else{
		scopeBuff[4][isound] = 0;
		snd += 0;
	}
	return snd;
} // synth

// -- MAIN AUDIO ROUTINE vvv
static int audioCallback(const void* inputBuffer, void* outputBuffer,
						unsigned long framesPerBuffer,
						const PaStreamCallbackTimeInfo* timeInfo,
						PaStreamCallbackFlags statusFlags, void* userData)
{
	float* out = (float*)outputBuffer;
	soundBuffer = framesPerBuffer;
	for (int i = 0; i < framesPerBuffer; i++) {
		isound = i;
		if(!play)t++;
		float snd;
		float bpm = 160;

		float secondsPerBeat = 60.0 / bpm;
		int transposer = t / (SAMPLE_RATE * secondsPerBeat);

		snd  = synth(t,bpm,((transposer/64)%2)?3:0);
		snd += drumSynth(t,bpm,1.0)/1.3;

		sndBuff[i] = snd;
		*out++ = snd*volume; // output
	}
	return paContinue;
}
// -- MAIN AUDIO ROUTINE ^^^

void init(){
	printf("GL and Synth initialization:");
	glClearColor(0.1,0.1,0.1,0);
	#ifndef NoMouse
	mouse.left=mouse.mid=mouse.right = 1;
	glutPassiveMotionFunc(mouseRoutine);
	glutMotionFunc(mouseRoutine);
	#endif // NoMouse

	glutKeyboardFunc(ButtonDown);
	glutKeyboardUpFunc(ButtonUp);
	glutMouseFunc(ButtonMouse);

	setupObjects();

	for(int i;i<1000;i++){
		if(i<=100){
			f[i]=0;
			drum[i]=0;
			retrigDrum[i]=0;
			retrigSynth[i]=0;
		}
		noise[i]= rand() % 2;
	}
	printf(" done.\n");
}

void positionCallback(int x, int y) {
	movingWindow = true;
}

void reshape(int width, int height) {
	glViewport(0, 0, width, height);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();

	// Set up a 2D coordinate system with the bottom-left corner at (0, 0)
	gluOrtho2D(0, width, 0, height);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	movingWindow = true;
}

int main(int argc, char** argv)
{

	printf("Starting.\n");
	// -- Audio
	printf("Pa_Initialize:");
	err = Pa_Initialize();
	if (err != paNoError) {
		fprintf(stderr, "PortAudio error: %s\n", Pa_GetErrorText(err));
		return 1;
	}else{printf(" success\n");}

	printf("Pa_OpenDefaultStream:");
	err = Pa_OpenDefaultStream(&stream, 0, 1, paFloat32, SAMPLE_RATE, 256, audioCallback, NULL);
	if (err != paNoError) {
		fprintf(stderr, "PortAudio error: %s\n", Pa_GetErrorText(err));
		return 1;
	}else{printf(" success\n");}

	printf("Pa_StartStream:");
	err = Pa_StartStream(stream);
	if (err != paNoError) {
		fprintf(stderr, "PortAudio error: %s\n", Pa_GetErrorText(err));
		return 1;
	}else{printf(" success\n");}
	// -- Audio

	// -- GL shit
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA);
	glutInitWindowSize(940,800);

	printf("Create Window:");
	int windowHandle = glutCreateWindow("OpenGL");
	if (windowHandle == 0){printf(" failed\n");}else{printf(" success\n");}

	init();

	glutCloseFunc(terminate);
	glutReshapeFunc(reshape);
	glutPositionFunc(positionCallback);
	glutDisplayFunc(display);
	glutMainLoop();
	// -- GL shit

	printf("Something went very wrong here.\nPress enter to exit.\n");
	getchar();
	printf("exiting...\n");
	terminate();

	return 0;
}
