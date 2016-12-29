#include <ncurses.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>

#define indexAtY (Y+first_char_Y-margin_top)                 //imlecin lines dizisinde tekabul ettigi satir
#define indexAtX (X-margin+first_char_X)          //imlecin lines dizisinde tekabul ettigi sutun
#define VERSION_MAJOR 0
#define VERSION_MINOR 0
#define VERSION_PATCH 4

/********************************************//**
 * _________ CHANGELOG (v0.0.4 ) _______________
 *
 * * FIXED fixed length problem in add_letter
 * * ADDED syntax highlighting for strings and comments (EXPERIMENTAL)
 * * FIXED enter key
 * * ADDED version number
 * * FIXED enter key, overflow bug
 * * FIXED page up, down keys
 * * FIXED length bug in copy function
 ***********************************************/

int COL, ROW, X, Y;
char name[80];
char filename[80];
int mode = 0; // 0 ekle, 1 secme anlaminda
int selection_start_x, selection_start_y, selection_end_x, selection_end_y;
int pasted = 0;

/* ilk karakterin x ve y pozisyonlari ekrana yazdirilacak ilk karakterin hangi satir
 * ve sutunda olacagibi belirliyor. */
int first_char_X = 0;   //! Ilk karakterin X pozisyonu
int first_char_Y = 0;   //! Ilk karakterin Y pozisyonu

int max_lines = 30;     //! Maksimum satir sayisi
char **lines;           //! Satir pointerlarini icinde tutan pointer dizisi
int margin = 4;         //! Satir numaralari icin bosluk
int margin_top = 2;
int margin_bottom = 2;
char *copy_buffer;      //! Kopyalanan kismi tutan char array
char *notify_buffer;
int notified = 0;
int is_in_comment = 0;
int is_in_one_line_comment = 0;
int is_in_string = 0;
int string_startX = 0;
int string_startY = 0;

void add_letter(char **line, int pos, char letter);
void remove_letter(char *line, int pos);
void update_screen();
void print_line_numbers();
void print_lines();
int get_last_line();
void add_new_lines_if_needed();
void print_menu();
void set_selection_end();
void copy();
void paste();
void key_handler(int ch);
void notify(char *message);
void notify_handler();
void print_header();
void kill_signal_handler();
void backspace();

int main() {
    int ch;
    initscr();        /* Start curses mode */

    getmaxyx(stdscr, ROW, COL);
    raw();        /* Line buffering disabled*/
    keypad(stdscr, TRUE);/* We get F1, F2 etc..*/
    noecho();        /* Don't echo() while we do getch */
    start_color();
    init_pair(1, COLOR_YELLOW, COLOR_BLACK);
    init_pair(2, COLOR_GREEN, COLOR_BLACK);

    signal(SIGINT, kill_signal_handler);
    signal(SIGTERM, kill_signal_handler);
    signal(SIGQUIT, kill_signal_handler);
    signal(SIGKILL, kill_signal_handler);

    lines = (char**)malloc(sizeof(char*) * max_lines);
    int ix;
    for(ix = 0; ix < max_lines; ++ix) {
        lines[ix] = malloc(sizeof(char) * 100);
        lines[ix][0] = '\0';
    }
    strcpy(filename, "Yeni Buffer");

    copy_buffer = malloc(sizeof(char) * 1000);
    copy_buffer[0] = '\0';
    notify_buffer = malloc(sizeof(char) * 1000);
    notify_buffer[0] = '\0';

    update_screen();
    move(margin_top, margin);

    while(1) {
        ch = getch();
        key_handler(ch);

        getyx(stdscr, Y, X);
        erase();
        move(Y, X);
        update_screen();
    }

    return 0;
}

void key_handler(int ch) {
    getyx(stdscr, Y, X);

    if(ch >= 32 && ch <= 126) { // 32: SPACE, 126: ~
        if(mode == 1){
            //      if selection mode is on, then delete the selected area
            char *temp;
            if(copy_buffer[0] != '\0'){
                temp = (char *) malloc(strlen(copy_buffer)+1);
                strcpy(temp,copy_buffer);
                copy();
                notified = 0;
                mode = 0;

                move(selection_end_y + margin_top, selection_end_x + margin);
                for(int i=0 ; i<strlen(copy_buffer) ; i++){
                    getyx(stdscr, Y, X);
                    backspace();
                }
                strcpy(copy_buffer, temp);
                free(temp);

            }else{
                copy();
                notified = 0;
                mode = 0;
                move(selection_end_y + margin_top, selection_end_x + margin);
                for(int i=0 ; i<strlen(copy_buffer) ; i++){
                    getyx(stdscr, Y, X);
                    backspace();
                }
                copy_buffer[0] = '\0';
            }
        }
        getyx(stdscr, Y, X);
        add_letter(&(lines[indexAtY]), indexAtX, ch);
        if((COL - X) == 1) {
            first_char_X++;
        } else {
            move(Y, X + 1);
        }
    }

    switch(ch) {
        case KEY_RESIZE:
            update_screen();
            refresh();
            break;
        case KEY_DOWN:
            if(Y + first_char_Y - margin_top != get_last_line()) {
                char *nextline = lines[indexAtY + 1];
                if(indexAtX > strlen(nextline)) {
                    if(strlen(nextline) < first_char_X) {
                        // if line is shorter than first char x
                        // scroll left
                        int offset;
                        if(strlen(nextline) <= 1){
                            offset = 0;
                        }else{
                            offset = 2;
                        }
                        if(Y != ROW - margin_bottom - 1) {
                            move(Y+1, margin + offset);
                        } else {
                            ++first_char_Y;
                            move(Y, margin + offset);
                        }
                        first_char_X = strlen(nextline) - offset;
                    } else {
                        if(Y != ROW - margin_bottom - 1) {
                            move(Y+1, margin + strlen(nextline) - first_char_X);
                        } else {
                            ++first_char_Y;
                            move(Y, margin + strlen(nextline) - first_char_X);
                        }
                    }
                } else {
                    if(Y != ROW - margin_bottom - 1) {
                        move(Y + 1, X);
                    } else {
                        ++first_char_Y;
                    }
                }
            }

            if(mode == 1) {
                set_selection_end();
            }
            break;
        case KEY_UP:
            if(Y + first_char_Y - margin_top != 0) {
                char *nextline = lines[indexAtY - 1];
                if(indexAtX > strlen(nextline)) {
                    if(strlen(nextline) < first_char_X) {
                        // if line is shorter than first char x
                        // scroll back
                        int offset;
                        if(strlen(nextline) <= 1){
                            offset = 0;
                        }else{
                            offset = 2;
                        }
                        if(Y - margin_top != 0) {
                            move(Y-1, margin + offset);
                        } else {
                            --first_char_Y;
                            move(Y, margin + offset);
                        }
                        first_char_X = strlen(nextline) - offset;
                    } else {
                        if(Y - margin_top != 0) {
                            move(Y-1, margin + strlen(nextline) - first_char_X);
                        } else {
                            --first_char_Y;
                            move(Y, margin + strlen(nextline) - first_char_X);
                        }
                    }
                } else {
                    if(Y - margin_top != 0) {
                        move(Y - 1, X);
                    } else {
                        --first_char_Y;
                    }
                }
            }
            if(mode == 1) {
                set_selection_end();
            }
            break;
        case KEY_LEFT:
            if(X != margin) {
                move(Y, X - 1);
            } else {
                if(first_char_X > 0) {
                    --first_char_X;
                }
            }
            if(mode == 1) {
                set_selection_end();
            }
            break;

        case KEY_RIGHT:
            if(X == COL - 1) {
                if(strlen(lines[indexAtY]) > COL - margin - 1) {
                    if(X + first_char_X - margin < strlen(lines[indexAtY])) {
                        ++first_char_X;
                    }
                }
            } else if(indexAtX < strlen(lines[indexAtY])) {
                move(Y, X+1);
            }
            if(mode == 1) {
                set_selection_end();
            }
            break;
        case KEY_BACKSPACE:
            // backspace key pressed
            if(mode == 1){
                //      if selection mode is on, then delete the selected area
                char *temp;
                if(copy_buffer[0] != '\0'){
                    temp = (char *) malloc(strlen(copy_buffer)+1);
                    strcpy(temp,copy_buffer);
                    copy();
                    notified = 0;
                    mode = 0;

                    move(selection_end_y + margin_top, selection_end_x + margin);
                    for(int i=0 ; i<strlen(copy_buffer) ; i++){
                        getyx(stdscr, Y, X);
                        backspace();
                    }
                    strcpy(copy_buffer, temp);
                    free(temp);

                }else{
                    copy();
                    notified = 0;
                    mode = 0;
                    move(selection_end_y + margin_top, selection_end_x + margin);
                    for(int i=0 ; i<strlen(copy_buffer) ; i++){
                        getyx(stdscr, Y, X);
                        backspace();
                    }
                    copy_buffer[0] = '\0';
                }

            }else if(mode == 0)
                backspace();
            break;

        case 10:
            // enter key pressed
            if(mode == 1){
                //      if selection mode is on, then delete the selected area
                char *temp;
                if(copy_buffer[0] != '\0'){
                    temp = (char *) malloc(strlen(copy_buffer)+1);
                    strcpy(temp,copy_buffer);
                    copy();
                    notified = 0;
                    mode = 0;
                    move(selection_end_y + margin_top, selection_end_x + margin);
                    for(int i=0 ; i<strlen(copy_buffer) ; i++){
                        getyx(stdscr, Y, X);
                        backspace();
                    }
                    strcpy(copy_buffer, temp);
                    free(temp);
                }else{
                    copy();
                    notified = 0;
                    mode = 0;
                    move(selection_end_y + margin_top, selection_end_x + margin);
                    for(int i=0 ; i<strlen(copy_buffer) ; i++){
                        getyx(stdscr, Y, X);
                        backspace();
                    }
                    copy_buffer[0] = '\0';
                }
            }
            getyx(stdscr, Y, X);


            add_new_lines_if_needed();
            if(Y == ROW - margin_bottom - 1) {
                ++first_char_Y;
                Y--;        //      satir birlestirme isleminde karisiklik olmamasi icin
            }
            if(indexAtY != get_last_line()) {
                int i;
                for(i = max_lines - 1; i > indexAtY + 1; --i) {
                    strcpy(lines[i], lines[i-1]);
                }
                move(Y+1, margin);
                lines[indexAtY + 1][0] = '\0';
            } else {
                move(Y+1, margin);
            }


            if(strlen(lines[indexAtY]) != indexAtX) {    // imlec, islem yapilan satirin son karakterinde degilse,
                //  imlecin oldugu yerden satir sonuna kadar olan kismi bir sonraki satir icine kopyala
                //sprintf(lines[5],"---%d != %d---", strlen(lines[indexAtY]), first_char_X);
                memcpy(lines[indexAtY + 1], &lines[indexAtY][indexAtX], strlen(lines[indexAtY])-indexAtX);
                lines[indexAtY+1][strlen(lines[indexAtY]) - indexAtX] = '\0';
                lines[indexAtY][indexAtX] = '\0';
            }
            first_char_X=0;     //Satir basina gecilir
            break;

        case KEY_F(1):
            if(mode == 0) {
                mode = 1;
                selection_start_x = indexAtX;
                selection_start_y = indexAtY;
                selection_end_x = indexAtX;
                selection_end_y = indexAtY;
            } else
                mode=0;
            break;

        case KEY_F(2):
            if(mode==0) {
                int a,b;
                getyx(stdscr, a, b);
                mode=2;
                erase();
                update_screen();
                mode = 0;
                strcpy(filename, name);
                FILE *dosya = fopen(name,"w+");
                for(int i=0; i<=get_last_line() ; i++) {
                    fprintf(dosya,"%s\n",lines[i]);

                }

                fclose(dosya);
                move(a,b);
            } else if(mode == 1) {
                copy();
            }
            break;

        case KEY_F(3):
            if(mode == 0){
                int a,b;
                getyx(stdscr, a, b);
                mode=3;
                erase();
                update_screen();
                mode = 0;
                if(strlen(name)>0){
                    FILE *ac = fopen(name,"r");
                    if(!ac){
                        char fail[80];
                        strcpy(fail, "\"");
                        strcat(fail, name);
                        strcat(fail,"\" acilamiyor.");
                        notify(fail);
                    }else{
                        int i;
                        for(i=0 ; i<=get_last_line() ; i++){
                            lines[i][0] = '\0';
                        }
                        i=0;
                        int c;
                        move(margin_top, margin);
                        first_char_Y = 0;
                        first_char_X = 0;
                        do{
                            c = getc(ac);
                            key_handler(c);
                        }while(c != EOF);
                        fclose(ac);
                        move(margin_top, margin);
                        first_char_X = 0;
                        first_char_Y = 0;
                        strcpy(filename, name);
                    }
                }

                move(a, b);
            }else if(mode == 1){
                copy();
                notified = 0;
                mode = 0;
                move(selection_end_y + margin_top, selection_end_x + margin);
                for(int i=0 ; i<strlen(copy_buffer) ; i++)
                    key_handler(KEY_BACKSPACE);


                notify("Kesildi...");

            }
            break;
        case KEY_F(4):
            if(mode == 1){
                //      if selection mode is on, then delete the selected area
                char *temp;
                if(copy_buffer[0] != '\0'){
                    temp = (char *) malloc(strlen(copy_buffer)+1);
                    strcpy(temp,copy_buffer);
                    copy();
                    notified = 0;
                    mode = 0;

                    move(selection_end_y + margin_top, selection_end_x + margin);
                    for(int i=0 ; i<strlen(copy_buffer) ; i++){
                        getyx(stdscr, Y, X);
                        backspace();
                    }
                    strcpy(copy_buffer, temp);
                    free(temp);

                }else{
                    copy();
                    notified = 0;
                    mode = 0;
                    move(selection_end_y + margin_top, selection_end_x + margin);
                    for(int i=0 ; i<strlen(copy_buffer) ; i++){
                        getyx(stdscr, Y, X);
                        backspace();
                    }
                    copy_buffer[0] = '\0';
                }
            }
            paste();
            break;
        case KEY_F(5):
            kill_signal_handler();
            break;
        case KEY_END:
            if(X < strlen(lines[indexAtY]) - first_char_X + margin) {
                if(strlen(lines[indexAtY]) < COL - margin) {
                    move(Y, strlen(lines[indexAtY]) + margin);
                } else {
                    move(Y, COL - 1);
                    first_char_X = strlen(lines[indexAtY]) - COL + margin + 1;
                }
            }
            break;
        case KEY_HOME:
            first_char_X = 0;
            move(Y, 4);
            break;
        case KEY_PPAGE:
            if(first_char_Y - (ROW / 2) > 0){
                first_char_Y -= ROW / 2;
                move(Y, margin);
            }else{
                move(margin_top, margin);
                first_char_Y = 0;
            }
            break;
        case KEY_NPAGE:
            if(first_char_Y + (ROW / 2) < get_last_line()){
                first_char_Y += ROW / 2;
            }
            move(Y, margin);
            break;
    }
}

/** \brief Stringe harf ekleyen fonksiyon
 *
 * \param *line     Harf eklenecek string
 * \param  pos      Yeni eklenecek harfin pozisyonu 0 olursa basina, strlen(line) olursa sonuna
 * \param letter    Eklenecek harf
 */
void add_letter(char **line, int pos, char letter) {
    // asagidaki uc satir, line overflow oluyor mu diye bakiyor
    // oluyorsa bellekten yeni yer aliyor line icin
    // line burada bir pointer
    if(strlen(*line) % 100 - 1 == 0 && strlen(*line) != 0) {
        *line = realloc(*line, sizeof(char) * strlen(*line) + 100);
    }

    char *newline;
    newline = malloc(sizeof(char) * strlen(*line) + 10);
    int i = 0;
    for(; i < pos; ++i) {       // posa kadar olan harfleri newline'a kopyala
        newline[i] = (*line)[i];
    }
    newline[pos] =  letter;     // pos'a yeni harfi ekle
    ++i;
    while((*line)[i-1] != '\0') {  // postan sonrasina string'in geri kalanini kopyala
        newline[i] = (*line)[i - 1];
        ++i;
    }
    newline[i] = '\0';          // newline'in sonuna \0 ekle
    strcpy((*line), newline);
}

/** \brief Stringden harf cikaran
 *
 * \param *line     Harf cikarilacak string
 * \param pos       Cikarilacak harfin pozisyonu
 * \note Bu fonksiyon  string.h'da bulunan basit bir memmove cagrisi ile yapilabilir.
 * Surada bir ornegi var: http://stackoverflow.com/a/5457726/2175857
 */
void remove_letter(char *line, int pos) {
    // bu fonksiyon calisiyor, nasil calistigini anlamadim yazmama ragmen ama calisiyor :-)
    char newline[1000];     // TODO: bu 1000 degismeli, malloc icin
    int i = 0, j = 0;
    while(line[i] != '\0') {        // ilk satir bitmedigi surece
        if(j != pos || i != pos) {  // pos pozisyonundaki karakterler haric
            newline[j] = line[i];   // butun karakterleri newline'a kopyala
            ++i;
            ++j;
        } else {
            ++i;
        }
    }
    newline[j] = '\0';  // newline'in sonuna \0 karakteri ekle
    strcpy(line, newline);  // newline'i line'a kopyala
}

/** \brief Ekrandaki her seyi bastan yaziyor.
 */
void update_screen() {
    getmaxyx(stdscr, ROW, COL);
    print_header();
    print_line_numbers();
    print_lines();
    print_menu();
    notify_handler();

}

/** \brief Satir numaralarini yazdiriyor.
 */
void print_line_numbers() {
    int i;
    int x, y;
    getyx(stdscr, y, x);        // imlecin ilk pozisyonunu al oraya geri donecek islem bitince
    attron(A_BOLD);             // yaziyi kalin yap

    for(i = margin_top; i < ROW - margin_bottom; ++i) {
        move(i, 0);             // imleci satirin basina getir
        if(i - margin_top + first_char_Y <= get_last_line() || i <= y) {
            // eger su anki satir en son satirdan kucukse ya da imlecin y'sinden kucukse
            printw("%3d", i + 1 - margin_top + first_char_Y);    // numara yazdir
        } else {
            printw("~");    // degilse ~ yazdir, vim gibi
        }
    }

    attroff(A_BOLD);    // yaziyi tekrar incelt
    move(y, x);         // imleci eski pozisyonuna geri dondur
}

/** \brief  Butun satirlari sirasiyle yazdiriyor.
 */
void print_lines() {
    int x, y;               // imlecin ilk pozisyonunu
    getyx(stdscr, y, x);    // bul, daha sonra geri donecek
    int i, j;
    int is_standout = false;
    is_in_comment = 0;
    is_in_string = 0;
    is_in_one_line_comment = 0;
    for(i = first_char_Y; i < first_char_Y + ROW - margin_bottom - margin_top ; ++i) {                // i, ilk karakterin Y'sinden ROW kadar uzaklasabilir
        is_in_one_line_comment = 0;
        is_in_string = 0;
        if(i >= max_lines)
            break;
        move(i - first_char_Y+margin_top, margin); // imleci o satirin basina getir
        for(j = first_char_X; j < first_char_X + COL - margin; ++j) {   // j, ilk karakterin X'inden COL kadar uzaklasabilir
            if(j > strlen(lines[i])) {
                break;
            }
            if(lines[i][j] == '\0'){
                if(is_in_one_line_comment){
                    is_in_comment = 0;
                    is_in_one_line_comment = 0;
                    attroff(COLOR_PAIR(1));
                }
                if(is_in_string){
                    is_in_string = 0;
                    attroff(COLOR_PAIR(2));
                }
                break;
            }
            if(((j == selection_start_x && i == selection_start_y) || (j == selection_end_x && i == selection_end_y))
                    && mode == 1 && !(selection_start_x == selection_end_x && selection_start_y == selection_end_y)) {
                if(!is_standout) {
                    attron(A_STANDOUT);
                    is_standout = 1;
                } else {
                    attroff(A_STANDOUT);
                    is_standout = 0;
                }
            }
            if(!is_in_comment && !is_in_string && lines[i][j] == '/' && lines[i][j+1] == '*'){
                attron(COLOR_PAIR(1));
                is_in_comment = 1;
            }
            if(!is_in_comment && !is_in_string &&  lines[i][j] == '/' && lines[i][j+1] == '/'){
                attron(COLOR_PAIR(1));
                is_in_comment = 1;
                is_in_one_line_comment = 1;
            }
            if(!is_in_comment && !is_in_string && lines[i][j] == '\"' && lines[i][j-1] != '\\'){
                attron(COLOR_PAIR(2));
                is_in_string = 1;
                string_startX = j;
                string_startY = i;
            }

            addch(lines[i][j]);         // satiri yazdir

            if(is_in_comment && !is_in_string && lines[i][j-1] == '*' && lines[i][j] == '/'){
                attroff(COLOR_PAIR(1));
                is_in_comment = 0;
            }
            if(!is_in_comment && is_in_string && !(string_startX == j && string_startY == i) && lines[i][j] == '\"' && lines[i][j-1] != '\\'){
                attroff(COLOR_PAIR(2));
                is_in_string = 0;
            }
        }
    }
    attroff(COLOR_PAIR(1));
    attroff(COLOR_PAIR(2));
    move(y, x); // imleci ilk pozisyonuna geri dondur
}

/** \brief Koddaki en son dolu satirin numarasini donduruyor.
 */
int get_last_line() {
    // son satiri sifira esitle
    // diger butun satirlar dolu mu diye kontrol et
    int last_line = 0;
    int i;
    for(i = 1; i < max_lines; ++i) {
        if(lines[i][0] != '\0') last_line = i;
    }
    return last_line;
}

/** \brief Bu fonksiyon bellekten yer aliyor gerektiginde.
 *
 * \param cur_line Bir sonraki satirin kacinci satir oldugu
 */
void add_new_lines_if_needed() {
    if(get_last_line() > max_lines - 5 || first_char_Y + Y > max_lines - 5) {
        // eger satirlarin bitmesine 5 satir kalmissa
        int i;
        max_lines += 50;    // max satir sayisini artir
        lines = (char**)realloc(lines, sizeof(char*) * max_lines);   // bellekten yeni yer al, dha buyuk
        for(i = max_lines; i >= max_lines - 50; --i) {              // yeni alinan yerleri
            lines[i] = malloc(sizeof(char) * 100);                  // yeni stringlerle doldur
            lines[i][0] = '\0';                                     // ilk karakter \0 olmali string olmasi icin
        }
    }
}

void print_menu() {
    int x, y, a, b;               // imlecin ilk pozisyonunu
    getyx(stdscr, y, x);    // bul, daha sonra geri donecek
    attron(A_BOLD);



    switch(mode) {
        case 0:
            for(int i=0 ; i<4; i++) {
                attron(A_STANDOUT);
                mvprintw(ROW-1, (COL/4)*i, " F%d ", i+1);
                attroff(A_STANDOUT);
            }
            mvprintw(ROW-1, (COL/4)*0+5, "Metin Sec");
            mvprintw(ROW-1, (COL/4)*1+5, "Farkli Kaydet");
            mvprintw(ROW-1, (COL/4)*2+5, "Ac");
            mvprintw(ROW-1, (COL/4)*3+5, "Yapistir");
            break;
        case 1:
            for(int i=0 ; i<4; i++) {
                attron(A_STANDOUT);
                mvprintw(ROW-1, (COL/4)*i, " F%d ", i+1);
                attroff(A_STANDOUT);
            }
            mvprintw(ROW-1, (COL/4)*0+5, "Secimi Kaldir");
            mvprintw(ROW-1, (COL/4)*1+5, "Kopyala");
            mvprintw(ROW-1, (COL/4)*2+5, "Kes");
            mvprintw(ROW-1, (COL/4)*3+5, "Yapistir");
            break;
        case 2:

            mvprintw(ROW-1, 0,"  Yazilacak dosya adi: "  );
            move(ROW-1, strlen("  Yazilacak dosya adi: ")+1);
            strcpy(name,"deneme");
            int i=0;
            attron(A_STANDOUT);

            int harf = getch();
            while(harf!=10) {
                if(harf >= 32 && harf <= 126) {
                    addch(harf);
                    //add_letter(&name, strlen(name), harf);
                    name[i] = harf;
                    i++;
                } else if(harf == KEY_BACKSPACE && i!=0) {
                    i--;
                    name[i] = '\0';
                    getyx(stdscr, a, b);
                    move(a, b-1);
                    attroff(A_STANDOUT);
                    addch(' ');
                    attron(A_STANDOUT);
                    move(a, b-1);
                }
                harf = getch();
            }
            if(i!=0)
                name[i] = '\0';
            else
                strcpy(name,"Yeni Metin Belgesi.txt");

            attroff(A_STANDOUT);

            break;
        case 3:
            mvprintw(ROW-1, 0,"  Acmak istediginiz dosyanin adini girin: "  );
            move(ROW-1, strlen("  Acmak istediginiz dosyanin adini girin: ")+1);
            strcpy(name,"deneme");
            i=0;
            attron(A_STANDOUT);

            harf = getch();
            while(harf!=10) {
                if(harf >= 32 && harf <= 126) {
                    addch(harf);
                    //add_letter(&name, strlen(name), harf);
                    name[i] = harf;
                    i++;
                } else if(harf == KEY_BACKSPACE && i!=0) {
                    i--;
                    name[i] = '\0';
                    getyx(stdscr, a, b);
                    move(a, b-1);
                    attroff(A_STANDOUT);
                    addch(' ');
                    attron(A_STANDOUT);
                    move(a, b-1);
                }
                harf = getch();
            }
            if(i!=0)
                name[i] = '\0';
            else
                strcpy(name,"");

            attroff(A_STANDOUT);

            break;

    }
    attroff(A_BOLD);
    move(y, x);
}

void set_selection_end() {
    int nx, ny;
    getyx(stdscr, ny, nx);
    selection_end_x = nx + first_char_X - margin;
    selection_end_y = ny + first_char_Y - margin_top;
}

void copy() {
    int y, x;
    getyx(stdscr, y, x);
    copy_buffer[0] = '\0';
    if(mode == 1) {
        if(selection_end_y < selection_start_y || (selection_end_y == selection_start_y && selection_end_x < selection_start_x)) {
            // swap end with start if end < start
            int sendx = selection_end_x;
            int sendy = selection_end_y;
            selection_end_x = selection_start_x;
            selection_end_y = selection_start_y;
            selection_start_x = sendx;
            selection_start_y = sendy;
        }
        int i, j;
        j = selection_start_x;
        while(1) {
            if(lines[selection_start_y][j] == '\0') {
                add_letter(&copy_buffer, strlen(copy_buffer), '\n');
                break;
            } else if(selection_start_y == selection_end_y && j == selection_end_x) {
                break;
            }

            add_letter(&copy_buffer, strlen(copy_buffer), lines[selection_start_y][j]);
            ++j;
        }
        for(i = selection_start_y + 1; i <= selection_end_y; ++i) {
            j = 0;
            while(1) {
                if(lines[i][j] == '\0') {
                    add_letter(&copy_buffer, strlen(copy_buffer), '\n');
                    break;
                }
                if(i == selection_end_y && j == selection_end_x) {
                    break;
                }
                add_letter(&copy_buffer, strlen(copy_buffer), lines[i][j]);
                ++j;
            }
        }
    }
    pasted = 0;
    move(y, x);
    notify("Kopyalandi...");
}

void paste() {
    int i;
    if(!pasted && mode == 1) {
        move(selection_start_y, selection_start_x + margin);
        selection_end_x = 0;
        selection_end_y = 0;
        selection_start_x = 0;
        selection_start_y = 0;
        pasted = 1;
        mode = 0;
    }
    for(i = 0; i < strlen(copy_buffer); ++i) {
        key_handler((int)(copy_buffer[i]));
    }
    notify("Yapistirildi...");
}

void notify(char *message) {
    int i;
    notify_buffer[0] = '\0';
    for(i = 0; i < strlen(message); ++i) {
        add_letter(&notify_buffer, strlen(notify_buffer), message[i]);
    }
    notified = 1;
}

void notify_handler() {
    if(notified) {
        int x, y;
        getyx(stdscr, y, x);

        move(ROW - 2, COL/2 - strlen(notify_buffer)/2 - 2);
        attron(A_STANDOUT | A_BOLD);
        printw("[ ");
        printw(notify_buffer);
        printw(" ]");
        attroff(A_STANDOUT | A_BOLD);

        notified = 0;
        move(y, x);
    }
}

void print_header() {
    int x,y;
    getyx(stdscr, y, x);
    int len;
    len = getmaxx(stdscr) + 1;
    char * header = (char *) malloc(len * sizeof(char));

    for(int i = 0 ; i<len ; i++) {
        header[i] = ' ';
    }
    header[len-1] = '\0';
    attron(A_STANDOUT | A_BOLD);
    mvprintw(0, 0, header);

    free(header);

    mvprintw(0, 2, "KOUPad %d.%d.%d", VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH);
    len = getmaxx(stdscr);
    mvprintw(0, (len - strlen(filename))/2, filename);

    attroff(A_STANDOUT | A_BOLD);
    move(y, x);
}

void kill_signal_handler(){
    endwin();
    printf("Cikiyor...\n");
    exit(EXIT_SUCCESS); // program kill oldugunda EXIT_SUCCESS ile cikiyor. :-)
}

void backspace(){
    if(X == margin) {
        // if x is at start
        if(first_char_X == 0) {
            // if x is in the start of line, both x = margin and first_char_X = 0
            //then we will jump to the previous line..
            if(indexAtY !=0) {
                // of course, there is no previous line if we are at the first one :P
                if(Y==margin_top) {
                    --first_char_Y;
                    Y++;
                }
                if(strlen(lines[indexAtY])!=0) {
                    lines[indexAtY-1] = (char*)realloc(lines[indexAtY-1], sizeof(char) * strlen(lines[indexAtY-1]) + strlen(lines[indexAtY]) + 5);
                    strcat(lines[indexAtY-1],lines[indexAtY]);

                    if(strlen(lines[indexAtY-1])+margin-strlen(lines[indexAtY]) <= COL){
                        move(Y-1,strlen(lines[indexAtY-1])+margin-strlen(lines[indexAtY]));
                    }else{
                        move(Y-1, COL-1);
                        first_char_X = strlen(lines[indexAtY-1])+margin-strlen(lines[indexAtY]) - COL +1;
                    }
                    for(int i=indexAtY ; i<get_last_line() ; i++) {
                        strcpy(lines[i],lines[i+1]);
                    }
                    lines[get_last_line()][0]='\0';
                } else {
                    move(Y-1,strlen(lines[indexAtY-1])+margin);

                    for(int i=indexAtY ; i<get_last_line() ; i++) {
                        //strcpy(lines[i],lines[i+1]);
                    }
                    if(indexAtY<get_last_line())
                        lines[get_last_line()][0]='\0';
                }
            }
        } else {
            first_char_X--;
            remove_letter(lines[indexAtY], indexAtX - 1);
        }
    } else {
        remove_letter(lines[indexAtY], indexAtX - 1);
        move(Y, X-1);
    }

}
