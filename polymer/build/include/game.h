// game.h

void	operatesector(short dasector);
void	operatesprite(short dasprite);
long	changehealth(short snum, short deltahealth);
void	changenumbombs(short snum, short deltanumbombs);
void	changenummissiles(short snum, short deltanummissiles);
void	changenumgrabbers(short snum, short deltanumgrabbers);
void	drawstatusflytime(short snum);
void	drawstatusbar(short snum);
void	prepareboard(char *daboardfilename);
void	checktouchsprite(short snum, short sectnum);
void	checkgrabbertouchsprite(short snum, short sectnum);
void	shootgun(short snum, long x, long y, long z, short daang, long dahoriz, short dasectnum, char guntype);
void	analyzesprites(long dax, long day);
void	tagcode(void);
void	statuslistcode(void);
void	activatehitag(short dahitag);
void	bombexplode(long i);
void	processinput(short snum);
void	view(short snum, long *vx, long *vy, long *vz, short *vsectnum, short ang, long horiz);
void	updatesectorz(long x, long y, long z, short *sectnum);
void	drawscreen(short snum, long dasmoothratio);
void	movethings(void);
void	fakedomovethings(void);
void	fakedomovethingscorrect(void);
void	domovethings(void);
void	getinput(void);
void	initplayersprite(short snum);
void	playback(void);
void	setup3dscreen(void);
void	findrandomspot(long *x, long *y, short *sectnum);
void	warp(long *x, long *y, long *z, short *daang, short *dasector);
void	warpsprite(short spritenum);
void	initlava(void);
void	movelava(char *dapic);
void	doanimations(void);
long	getanimationgoal(long animptr);
long	setanimation(long *animptr, long thegoal, long thevel, long theacc);
void	checkmasterslaveswitch(void);
long	testneighborsectors(short sect1, short sect2);
long	loadgame(void);
long	savegame(void);
void	faketimerhandler(void);
void	getpackets(void);
void	drawoverheadmap(long cposx, long cposy, long czoom, short cang);
long	movesprite(short spritenum, long dx, long dy, long dz, long ceildist, long flordist, long clipmask);
void	waitforeverybody(void);
void	searchmap(short startsector);
void	setinterpolation(long *posptr);
void	stopinterpolation(long *posptr);
void	updateinterpolations(void);
void	dointerpolations(void);
void	restoreinterpolations(void);
void	printext(long x, long y, char *buffer, short tilenum, char invisiblecol);
void	drawtilebackground (long thex, long they, short tilenum, signed char shade, long cx1, long cy1, long cx2, long cy2, char dapalnum);
