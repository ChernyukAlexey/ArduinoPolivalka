/* 
  ПРОЕКТ ПОЛИВАЛКА !!!  made by: ACHERN
  Last edit: 18/06/2017 - 12:00  - не забудь залить на ГуглДиск: 
  При инициализации происходит запуск 2-ух будильников (например:вторник и пятница) 
  При срабатывании каждого из будильников запускается таймер безопасности на MainTimePeriod секунд.
  Потом соответственно он останавливается выключая насос в любом случае (это защита!)
  При запуске таймера безопасности запускается выполнение циклов полива.
  Возможны варианты логики: 
  1. Запускаем циклический таймер каждые 20 секунд. 
  В течении этих 20 секунд включаем насос на 5 секунд.
  По истичении нескольких циклов таймер можно деактивировать!
  2. В течении 300 секунд (реализовано сейчас)
  запускаем некоторое количество функций включения|выключения
  с использованием Alarm.Delay()
  Тайминги работы/простоя берем из массивов on_periods_N и off_periods_N (где N=1 or N=2)
  
  Справочно: - посмотреть на досуге склеивание строк
  1.Выделяем нужное количество памяти
  2.Склеиваем и возвращаем указатель на начало блока памяти
// Использовать fh[bdyst библиотеки Time, Bounce,  (сделаны  изменения в файлы libraries\Time\DateStrings.cpp)
   
  #include <stdlib.h>
  #include <string.h>
  char* concat(char *s1, char *s2)
  {
    char *result = malloc(strlen(s1)+strlen(s2)+1);//+1 нужен для нуль-терминатора строки.
    strcpy(result, s1);
    strcat(result, s2);
    return result;
  }
  
  
*/

/* 
   Каждую среду и воскресенье (по будильнику Alarm.alarmRepeat) в 9_30 осуществляем полив. 
   isTimerWeekOn = true - признак что будильник запущен
   Происходит запуск функции: PumpCycleStart (таймер на 100 секунд признак запуска: isTimer1On )
   Примечание: Для отладки используем запуск PumpCycleStart через кнопку!!!
   Она осуществляет запуск серии импульсов полива (включения насосов)
*/      


#define TP_OUT 3    // ОПРЕДЕЛЕНИЕ СПОСОБА ВЫВОДА ИНФОРМАЦИИ !!!   0-non/1-Com_port/2-B_Tooth/3-LED SSD
#define TESTMODE 0  // ВКЛЮЧЕНИЕ ТЕСТОВОГО РЕЖИМА 0-выкл   1-вкл

// #define _A2_ "2A21:35 ED01S3 A" // каждый день в 21:35  алгоритм S3 (включен)
// #define _A3_ "3A10:30 EW05S1  " // каждый четверг в 10:30 алгоритм S1 (выключен) 
// Прим.: 02-понедельник, а 01-воскресенье  // для day of week, sunday is day 1

#define _A1_ "1A22:35 EW06S1 A" // 
#define _A2_ "2A21:35 ED01S3  " // 
#define _A3_ "3A10:30 EW05S1  " // 

byte MainTimePeriod = 250; // время основного цикла полива
// Алгоритм полива S1
byte on_periods_1[5] = {20,20,20,0,0};  // время работы
byte off_periods_1[5] = {20,20,30,20,20}; // время простоя
// Алгоритм полива S2
byte on_periods_2[3] = {20,20,20};  // время работы
byte off_periods_2[3] = {10,20,30}; // время простоя

#define _AlarmOnFunc_ WeekAlarmOn // Функция, запускаемая таймерами Alarm 

#include <Wire.h>   // интерфейс I2C
#include <DS3231.h> // USED  DS3231_TEST  !!!
#include <DS1307RTC.h>
#include <Time.h>
#include <TimeAlarms.h>
// подключаем библиотеку дребезга "Bounce"
#include <Bounce.h>

#if TP_OUT == 3 // ДЛЯ РАБОТЫ С ДИСПЛЕЕМ SSD1306 интерфейс SPI  (смотреть примеры) 
    #include "U8glib.h"
// Для удобства поменяли 10 на 8
   U8GLIB_SSD1306_128X64 u8g(10, 11, 8, 9, 12);
    
    /* Подключение:
    U8GLIB_SSD1306_128X64 u8g(8, 11, 10, 9, 12);
    MODULE`     UNO
    GND         GND
    VCC         +3.3V
    D0 (SCK)    D8
    D1 (MOSI)   D11
    RST         D12
    DC (A0)     D9
    CS (CS)     D10
    */
    #define LINE_MAX 30 
    uint8_t line_buf[LINE_MAX] = "   =POLIVALKA=";
    uint8_t line_pos = 0;
    // setup a text screen to support scrolling
    #define ROW_MAX 12
    uint8_t screen[ROW_MAX][LINE_MAX];
    uint8_t rows, cols;
    // line height, which matches the selected font (5x7)
    #define LINE_PIXEL_HEIGHT 7    
#endif 

// значения пинов с кнопками
#define BUTTON1 2
#define BUTTON2 4
// значения пинов с насосами
#define PUMP1 7
#define PUMP2 6
// Зуммер
#define BEEP_PIN 3

#if TP_OUT == 2  // Для режима БТ - используем программный порт
 #include <SoftwareSerial.h>
#endif

DS3231 Clock;
char Time_str[22] = " hh:mm:ss dd/mm/yy#n "; // Строка формата времени: 21 символ + '\0'
char str_i[30];          // для облегчения вывода отладочных сообщений
// Коды будильников и таймеров (всего 3)
AlarmID_t myAlarm[2];
AlarmID_t myTimer[2];
/* массив указателей массивов строк будильников */
char* Alarms[3]={_A1_,_A2_,_A3_};      // для char размер массива должен быть больше на 1
// Дни недели/время по которым срабатывает полив получим из массивов строк будильников
// ( первый элемент массива - 1ый будильник, соответственно 2 для второго)
byte ADay[3];    // ={1,7, 15};   // Число месяца (по необходимости)
byte AHour[3];   // ={9,10};      // Час1/Час2 
byte AMinute[3]; // ={28,45};     // Минута1/Минута2 
timeDayOfWeek_t ADoW[3];  // День недели  // Внимание!  Воскресенье это - 1

byte i_s; // меняем каждые 5 секунд с 0 на 1 ??=

// Количество элементов должно совпадать со значением N_Cycles)
// Определим сколько периодов будет в цикле - Кол циклов вкл/выключений размер массива sizeof[]
byte N_Cycles = sizeof(on_periods_1)-1;
byte CycleNum=0; //  Текущий номер цикла 

//создаем объект класса Bounce. Указываем пин, к которому подключена кнопка, и время дребезга в мс.
Bounce bouncer1 = Bounce(BUTTON1,5); 
Bounce bouncer2 = Bounce(BUTTON2,5); 

/*  --  */
bool Century = false;
bool h12;
bool PM;
bool ADy, A12h, Apm;

bool isPumpOn = false;      // флаг включения насоса
bool isTimer1On = false;    // флаг того что таймер(1) запущен (основной цикл полива MainTimePeriod секунд )
bool isTimer2On = false;    // флаг того что таймер(2) запущен (действия внутри цикла таймера1)  - не используем
bool isWeekAlarmOn = false; // факт срабатывания любого из недельных будильников

//задаем начальное состояние светодиода "выключен"
int Pump1Value = LOW;
int Pump2Value = LOW;
//float currentTime = 0;
float SyncTime = 0;

#if TP_OUT == 2 // Програмный порт  10/11 уже заняты !!!
 SoftwareSerial mySerial =  SoftwareSerial(5,13); // create object
#endif 

void setup() {
//определяем режимы работы пинов
  pinMode(BUTTON1,INPUT);
  pinMode(BUTTON2,INPUT);
  pinMode(PUMP1,OUTPUT);
  pinMode(PUMP2,OUTPUT);
  pinMode(BEEP_PIN,OUTPUT);
  
  digitalWrite(BEEP_PIN,LOW);
  digitalWrite(PUMP1,LOW);
  digitalWrite(PUMP2,LOW);
/* Включим подтяжку на кнопки - Активный низкий */
  digitalWrite(BUTTON1,HIGH);
  digitalWrite(BUTTON2,HIGH);
  
// Start the I2C interface SCL->A5   SDA->A4
  Wire.begin();
// УСТАНОВКА ЧАСОВ !!! setTime(int hr,int min,int sec,int day, int month, int yr);
//setTime(18, 20, 50, 30, 12,  16); - установка часов ардуино
// Установка часов РТС   
 /*      
        Clock.setSecond(50);  //Set the second 
        Clock.setMinute(58);  //Set the minute 
        Clock.setHour(11);    //Set the hour 
        Clock.setDoW(4);    //Set the day of the week 1 - Sunday!!!
        Clock.setDate(28);  //Set the date of the month
        Clock.setMonth(6);  //Set the month of the year
        Clock.setYear(17);  //Set the year (Last two digits of the year)           
  */
#if TP_OUT == 1  // COM
  Serial.begin(9600);
  // Ждём, пока запустится последовательный порт
  while (!Serial);
#elif TP_OUT == 2
  mySerial.begin(9600);  // Enable communication prog port
  while (!mySerial);
  mySerial.println("Hello, bluetooth?");  
#elif TP_OUT == 3  // ДЛЯ ДИСПЛЕЯ LED
  // u8g.setFont(u8g_font_fub30);
  // u8g.setFont(u8g_font_profont11);
  // u8g.setFont(u8g_font_unifont); // 16 позиций в строке для моего дисплея
  u8g.setFont(u8g_font_5x7);
  // u8g.setFont(u8g_font_9x15); 
  u8g.setFontPosTop();
  
  // calculate the number of rows for the display
  rows = u8g.getHeight() / u8g.getFontLineSpacing();
  if ( rows > ROW_MAX )  rows = ROW_MAX;  
  // estimate the number of columns for the display
  cols = u8g.getWidth() / u8g.getStrWidth("m");
  if ( cols > LINE_MAX-1 ) cols = LINE_MAX-1;  
  clear_screen();               // clear screen
  delay(1000);                  // do some delay
  exec_line();                  // place the input buffer into the screen
  reset_line();                 // clear input buffer
#endif   

  MyPrint_P(PSTR("START SYSTEM"),1,0);
  Alarm.delay(3000);
 // Устанавливаем функцию для получения времени с часов
 // Библиотека получает с модуля время один раз и далее ведёт отсчёт по внутренним часам Arduino,
 // синхронизируясь с часами по мере необходимости
  setSyncProvider(RTC.get);  // Библиотека не получает время с часов каждый раз. Задаётся функция для подключения к модулю часов
  if(timeStatus()!= timeSet)  { MyPrint_P(PSTR("Can't set RTC!"),1,1);}  else { MyPrint_P(PSTR("RTC SET TIME"),0,1); }
  setSyncInterval(1000);     // set the number of seconds between re-sync  
  
 // MyPrint_P(PSTR("\n");  
 // if (TP_OUT == 1) {Serial.end();};   if (TP_OUT == 2) {mySerial.end();};
  tone(BEEP_PIN,3000,50);
  Alarm.delay(3000);   
  
  // СОЗДАЕМ И ВЗВОДИМ БУДИЛЬНИКИ !!!  
  #if TESTMODE == 0
    LoadTunes();      // РАБОЧИЙ РЕЖИМ
  #else
    MyPrint_P(PSTR("TEST MODE ON"),0,1);
    Alarm.delay(3000);
    LoadTestTunes();  // ТЕСТИРУЕМ !!! 
   // Serial.print("hour()=");
   // Serial.println(weekday()); // "это почему то помогло при ошибках вылета компилятора
  #endif
}

void loop() { 

// если сменилось состояние кнопки1
// если самих кнопок нет, то выводы надо бросить на ноль иначе возможны "случайные срабатывания"
  if ( bouncer1.update() ) {
    //если считано значение 1
    if ( bouncer1.read() == LOW) { // Activ LOW
       //если свет был выключен, будем его включать и наоборот
       if ( Pump1Value == LOW ) { Pump1Value = HIGH; } else {Pump1Value = LOW;}
       // Если включаем то запускаем таймер на 1мин потом гасим
       if (Pump1Value == HIGH) { StartPump(); /* ЗАПУСК НАСОСА */ };
       if (Pump1Value == LOW) { StopPump();   /* ОСТАНОВ НАСОСА ЭКСТРЕННЫЙ */ };
     // called once after 10 seconds 
    }
  } 
// Если нажали кнопку - старт последовательности Таймеров 
  if (bouncer2.update() ) {
     if ( bouncer2.read() == LOW) {
         //если  выключен, будем его включать
         if ( Pump1Value == LOW ) { Pump1Value = HIGH; WeekAlarmOn(); /*PumpCycleStart();*/ }
     }
  } 
  
// Alarm.delay(10000);
// Каждые 5 секунд меняем 0 или 1 - Это потом придумаем!
  i_s = Clock.getSecond()%5%4%3;    // остаток от деления  меняем каждую секунду отображение даты                                          

// Каждые 0.55 сек проверяем минуты
  if ((millis() - SyncTime) >= 1000) 
      {
       SyncTime = millis();
     #if TP_OUT == 3  // LED         
       add_sets_to_screen();
     #endif   
     
       if (Clock.getMinute() == 59)  { /* MyPrint_P(PSTR("59_M_OVER"),1,1); */ tone(BEEP_PIN,500,700); Alarm.delay(20000);};
      }   
   
/* во время активности таймера1 сделаем интервалы включения и остановки */
  if (isTimer1On==true /*&& isTimer2On==false*/ && isWeekAlarmOn == true) {  
     /* первый элемент массива с номером 0 */
          if (CycleNum <= N_Cycles /* выкл */ ) { 
                isTimer2On = true;
                // Включили на нужное число секунд
                StartPump_Ns(on_periods_1[CycleNum]);
                // Сделали соответствующую паузу в секундах
                Alarm.delay(off_periods_1[CycleNum]*1000);
                CycleNum++;
                if (CycleNum==N_Cycles+1) {isTimer2On = false; }   // Малый Цикл закончен
           }                  
   };  
 Alarm.delay(1);
// перехват кода от BLUETOOTH Модуля HC-06(05) и запуск команды   
  #if TP_OUT == 2  // BT         
    MySerialR();   // Сейчас подглючивает !!!  Надо сделать помехозащиту 
  #endif  
   
}  // END LOOP

/* является параметром будильника */
void WeekAlarmOn(){
        // Признак того что недельный будильник сработал и включаем насос!
        isWeekAlarmOn = true; 
        MyPrint_P(PSTR("RUN W_Alarm()"),1,1);  
        PumpCycleStart(); 
      }        

void PumpCycleStart(){  
  MySignal(); // Пищим!
  if (!isPumpOn && !isTimer1On ) { 
//     если насос не включен и таймер не взведен - то запускаем
//     записываем значение вкл/выкл на пин со светодиодом/насосом
      MyPrint_P(PSTR("Start P_Cycle"),1,1);     
//     Запускаем таймер isTimer1On
//     Через MainTimePeriod секунд будет запущена функция выключения этого цикла 
//     В это время мы можем включать и выключать насос с использованием других таймеров либо остановок   
       if (isTimer1On == false) { 
           Alarm.timerOnce(MainTimePeriod, StopPumpTimer); 
           CycleNum = 0;
           isTimer1On = true;
         }
       else {MyPrint_P(PSTR("Timer1(main) is already active! "),1,1);}
  } 
  else   { MyPrint_P(PSTR("W: PumpisOn or isTimer_1_On! "),1,1); }    
}  // end PumpCycleStart()

/* Запуск для тестирования */
void StartPump(){
  tone(BEEP_PIN,3000,100);
  if (!isPumpOn) {
     //записываем значение вкл/выкл на пин со светодиодом/насосом
     Pump1Value = HIGH;
     digitalWrite(PUMP1,Pump1Value);  // отдельный пин управления насосом
     MyPrint_P(PSTR("Start Pump"),1,1);
     
     isPumpOn = true;  // Признак работы насоса
  } else { MyPrint_P(PSTR("W: Pump is already worked!"),1,1); }
}

/* void StartPump_5s() {
  CycleNum++;
  StartPump();
  Alarm.delay(5000);
  StopPump();
}*/

/* включение насоса на заданное количество секунд */
void StartPump_Ns(byte sec) {
  StartPump();
  Alarm.delay(sec*1000);
  StopPump();
}

/* Резкая Остановка насоса */
void StopPump(){
   tone(BEEP_PIN,1000,500);
   MyPrint_P(PSTR("Stop Pump "),1,1);
   Pump1Value = LOW;
   digitalWrite(PUMP1,Pump1Value); 
   if (isPumpOn == false) {MyPrint_P(PSTR(" W: Pump not be active! "),0,1); }  
   isPumpOn = false;
   if (isTimer1On == true) {MyPrint_P(PSTR(" W: Timer_1(200s) is already active! "),0,1);}
}

/* Остановка насоса по таймеру, используется в основном цикле*/
void StopPumpTimer(){
       tone(BEEP_PIN,1500,500);
       Alarm.delay(500);
       tone(BEEP_PIN,500,1000); 
       isTimer1On = false;  // MainTimePeriod - секундный таймер
       isTimer2On = false;  // признак того что циклический таймер выключен           
       //  Alarm.disable( myTimer1); myPrint_3("Alarm.disable(myTimer1)", " *** STOP ALARM *** ","\n");
       MyPrint_P(PSTR("Stop P_Cycle"),1,1); //,"char(MainTimePeriod)" ,"\n");           
       StopPump();             // на всякий случай останавливаем насос
       CycleNum = 0;           // обнуляем счетчик для следующих запусков
       isWeekAlarmOn = false;  // Программа "Будильника" отработала. Можно включать новый.
    // для проверки !!
    // Alarm.timerRepeat(3600, WeekAlarmOn);          
}
 
/* ПИЩАЛКА!!! */
 void MySignal() 
 {   uint8_t i;
     uint8_t DelaySound = 200; // Пауза 0.2 секунда
    int t[7] = {1915,1519,1432,1275,1700,1136,1014};
    // MyPrint_P(PSTR("MySign_RUN"),1,1);
    for (i = 0; i<7 ; i++) { 
       tone(BEEP_PIN, t[i]);       
       if (i==2 || i==5) {DelaySound-=100;} else {DelaySound = 200; }
       Alarm.delay(DelaySound); 
    }         
    noTone(BEEP_PIN); // Выключаем звук       
 }
            
//    Максимальное число будильников - 7 !!!
//    Alarm.timerOnce(10, StopPump); MyPrint_P(PSTR("Alarm.timerOnce(10, StopPump) ");   MyPrint_P(PSTR("\n");
//    Alarm.timerOnce(30, StartPump); MyPrint_P(PSTR("Alarm.timerOnce(30, StartPump) "); myPrint("\n");             
//    ВАРИАНТ 1 (функция StartPump_5s() запускается через 20 сек 
//    пока не сработает Таймер1 и не отключит ее 
//    иначе Этот таймер будет крутится вечно!
//    id = Alarm.getTriggeredAlarmId(); myPrint_3(String(id), " - Timer ID ","\n");                             

/* отформатируем время 0 - полное время_дата / 1-только дату / 2-день недели */
/* Запись текущего времени в строку Time_str */
void PutTimeStr(uint8_t par) {
  String S1 = " ";
  // Формат: _hh:mm:ss_dd/mm/yy#n  (20 символов)
  S1 = S1 + Dig(Clock.getHour(h12, PM)) + ":" ; 
  S1 = S1 + Dig(Clock.getMinute());
  S1 = S1 + ":" + Dig(Clock.getSecond()); 
  S1 = S1 + " "+Dig(Clock.getDate()) + "/"; 
  S1 = S1 + Dig(Clock.getMonth(Century)) + "/"; 
  S1 = S1 + Dig(Clock.getYear()); 
  S1 = S1 + "#" + String(Clock.getDoW());
  // только время с секундами (отсчет с нулевой позиции)
  if (par == 1) { S1 = S1.substring(0,9); }  
  // время с hh:mm 
  if (par == 2) { S1 = S1.substring(0,5); }  
  // только дату   
  if (par == 3 ) { S1 = S1.substring(9,20); }
  // S1 = S1+" ";  
  if (par == 10 ) { S1 = ""; }  // ЕСЛИ 10 - НЕ ВЫВОДИМ ВРЕМЯ
  S1.toCharArray(&(Time_str[0]), 22);  
 }

/*void PutTimeStr_n(uint8_t par) {
  *Time_str = Time_str[0];
      Time_str[0] = 'G'; // char(round(Clock.getHour(h12, PM)/10)); // Десятки
      Time_str[1] = char(Clock.getHour(h12, PM)%10); // Единицы
      Time_str[2] = ':';
      Time_str[3] = char(Clock.getMinute()); // Десятки
      Time_str[4] = char(Clock.getMinute()%10); // Единицы
      Time_str[5] = ':';
      Time_str[6] = char(round(Clock.getMinute()/10)); // Десятки
      Time_str[7] = char(Clock.getSecond()%10); // Единицы
      Time_str[8] = '5';
      Time_str[9] = '\0'; */

  // Формат: _hh:mm:ss_dd/mm/yy#n  (20 символов)
 /* S1 = S1 + Dig(Clock.getHour(h12, PM)) + ":" ; 
  S1 = S1 + Dig(Clock.getMinute());
  S1 = S1 + ":" + Dig(Clock.getSecond()); 
  S1 = S1 + " "+Dig(Clock.getDate()) + "/"; 
  S1 = S1 + Dig(Clock.getMonth(Century)) + "/"; 
  S1 = S1 + Dig(Clock.getYear()); 
  S1 = S1 + "#" + String(Clock.getDoW());
  // только время с секундами (отсчет с нулевой позиции)
  if (par == 1) { S1 = S1.substring(0,9); }  
  // время с hh:mm 
  if (par == 2) { S1 = S1.substring(0,5); }  
  // только дату   
  if (par == 3 ) { S1 = S1.substring(9,20); }
  // S1 = S1+" ";  
  S1.toCharArray(&(Time_str[0]), 22);  
 
 }*/


/* вставляет лидирующий ноль для печати даты/время */
// char* Dig(int p) { if (p < 10) { return strcat('0',char(p)); } else return *char(p); };
String Dig(uint8_t p) 
  { if (p < 10) { return "0" + String(p); }  else return String(p); }

#if TP_OUT == 3    // to LED
// clear entire screen, called during setup

void clear_screen(void) {
  uint8_t i, j;
  for( i = 0; i < ROW_MAX; i++ )  //  
    for( j = 0; j < LINE_MAX; j++ )
      screen[i][j] = 0;  
}

// append a line to the screen, scroll up
void add_line_to_screen(void) {
  uint8_t i, j;
  for( j = 0; j < LINE_MAX; j++ )  // Поднимаем строку вверх (каждый символ строки поднимаем вверх)
    for( i = 3; i < rows-1; i++ )  // Вывод с четвертой строки - первые три под время
      screen[i][j] = screen[i+1][j];
  
  for( j = 0; j < LINE_MAX; j++ )  // в последнюю строку выводим буфер
    screen[rows-1][j] = line_buf[j];
}

void add_sets_to_screen(void) {
  uint8_t i, j;
  PutTimeStr(0); // полное время_дата_дн
  for( j = 0; j < LINE_MAX; j++ )  
    //for( i = 0; i < 2; i++ )
     { //screen[0][j] = Time_str[j];
     // обрежем время до минут и соединим с настройкой
       if (j < 15) { 
         screen[0][j] = Alarms[0][j+1];
         screen[1][j] = Alarms[1][j+1]; } 
       else if ( j > 24 ) { // секунды не отображаем
         screen[0][j] = '/';
         screen[1][j] = '/'; }
       //  if (j > 26) { screen[1][j] = Time_str[j-18]; }
       else {  
         Time_str[j-6] = 0;  // {  /*tone(BEEP_PIN,3000,2000);*/ }
         screen[0][j] = Time_str[j-15];
         screen[1][j] = Time_str[j-5]; }  
     } 
// U8GLIB picture loop
  u8g.firstPage();  do {  draw();} while( u8g.nextPage() );
  }

// U8GLIB draw procedure: output the screen
void draw(void) {
  uint8_t i, y;
  // graphic commands to redraw the complete screen are placed here    
  y = 0;       // reference is the top left -1 position of the string
  y--;           // correct the -1 position of the drawStr 
  for( i = 0; i < rows; i++ )
  {
    u8g.drawStr( 0, y, (char *)(screen[i]));
    y += u8g.getFontLineSpacing();
  }
}

void exec_line(void) {
  // echo line to the serial monitor
  // Serial.println((const char *)line_buf);  
  // add the line to the screen
  add_line_to_screen();
  add_sets_to_screen();
  
  // U8GLIB picture loop
  u8g.firstPage();  
  do {
    draw();
  } while( u8g.nextPage() );
}

// clear current input buffer
void reset_line(void) { 
      line_pos = 0;
      line_buf[line_pos] = '\0';  
}

// add a single character to the input buffer 
void char_to_line(uint8_t c) {
      line_buf[line_pos] = c;
      line_pos++;
      line_buf[line_pos] = '\0';  
}

void read_line_str(char *str) {
    uint8_t c;
    uint8_t n;
    for (n=0; *str!='\0'; str++) 
   { 
   c=str[n];
   // Serial.println(c);
    if ( line_pos >= cols-1 ) {
      exec_line();
      reset_line();
      char_to_line(c);
    } 
    else if ( c == '\n' ) {
      // ignore '\n' 
    }
    else if ( c == '\r' ) { // cbvdjk
      exec_line();
      reset_line();
    }
    else {
      char_to_line(c);
    }
   };
   
   exec_line();
   reset_line();
 }
#endif     

/* функция для тестирования работы */
#if TESTMODE == 0
void LoadTunes()
  {
   String S;
   byte j;
  /*
  #define _A1_ "1A10:15 ED00S1 A" // будильник 1 каждый день (алгоритм S1) активен
  #define _A2_ "2A14:30 EW01S2  " // будильник 2 каждый понедельник (выключен)
  #define _A3_ "3A14:30 EM25S1 A" // будильник 3 каждое 25 число месяца
  */ 
    for (byte i = 0; i < 3; i++){ 
      S = Alarms[i]; // Читаем настроечную строку из массива
      myPrint(Alarms[i],0,1);
      Alarm.delay(1000);
      if (S.charAt(15)=='A') // Если активен
      { 
        AHour[i] = byte(S.substring(2,4).toInt());   // часы
        AMinute[i] = byte(S.substring(5,7).toInt()); // минуты
      // ADoW[i] = byte(S.substring(9,10).toInt());  // Периодичность Месяц/Неделя/День
      // Ежедневный таймер
        if (S.charAt(9) == 'D') {  
           myAlarm[i] = Alarm.alarmRepeat(AHour[i], AMinute[i],20,_AlarmOnFunc_);
          // str_i[0] = '\0'; // strcpy(&(str_i[0]),   strcat(str_tmp, Time_str);
           strcat(str_i,"ALARM ");
           str_i[6] = S.charAt(0);
           strcat(str_i," RUN!");
           myPrint(str_i,0,1); 
          // str_i[0] = '\0';                      
        }
      // Еженедельный таймер (в строке настройки используем 1-понедельник и т.д.) 
        if (S.charAt(9) == 'W') {  
               switch (S.substring(11,12).toInt()) {
               // dowInvalid, dowSunday, dowMonday, dowTuesday, dowWednesday, dowThursday, dowFriday, dowSaturday
                case 2:  ADoW[i]= dowMonday;     break;  // понедельник
                case 3:  ADoW[i]= dowTuesday;    break;  // 
                case 4:  ADoW[i]= dowWednesday;  break;  // 
                case 5:  ADoW[i]= dowThursday;   break;  // Четверг
                case 6:  ADoW[i]= dowFriday;     break;  // 
                case 7:  ADoW[i]= dowSaturday;   break;  // 
                case 1:  ADoW[i]= dowSunday;     break;  // Воскресенье  // хотя для day of week, sunday is day 1
                default: ADoW[i]= dowSunday;  
               } 
          // Запускаем будильник (по дням недели)               
          myAlarm[i] = Alarm.alarmRepeat(ADoW[i], AHour[i], AMinute[i],20,_AlarmOnFunc_);
          MyPrint_P(PSTR("WA START"),1,1);
         }    
      // ежемесячный таймер  
         if (S.charAt(9) == 'M') {  
              myAlarm[i] = Alarm.alarmRepeat(ADoW[i], AHour[i], AMinute[i],20,_AlarmOnFunc_);
              MyPrint_P(PSTR("MA START"),1,1);
            }
       } 
       else 
       { /* myPrint(myPrint(itoa(AHour[1],str_i,10));*/ 
          strcat(str_i,"ALARM ");
          str_i[6] = S.charAt(0);
          strcat(str_i," DISABL");
          myPrint(str_i,1,1);
       }    
     // Заполним массив str_i[30] нулями
     j=0;
     while ( j < sizeof(str_i)-1) { str_i[j]=0; j++; } 
    } // цикл по строкам настроек
  }   // end LoadTunes()
  
#else
// Устанавливает будильники 1 и 2 на запуск через 2 и 5 минут после загрузки/сброса
void LoadTestTunes()  // запустим будильники через 2 и через 8 минут после старта в текущий день недели!
 {   uint8_t i;
     char t[3];
    // определим текущий день недели   
    for (i = 0; i < 2; i++)
    { t[0] ='\0';
      switch (weekday()) {
               // dowInvalid, dowSunday, dowMonday, dowTuesday, dowWednesday, dowThursday, dowFriday, dowSaturday
                case 1:  ADoW[i]= dowSunday;    strcat(t,"Su");  break;  // Воскресенье  // day of week, sunday is day 1
                case 2:  ADoW[i]= dowMonday;    strcat(t,"Mo");  break;  // понедельник
                case 3:  ADoW[i]= dowTuesday;   strcat(t,"Tu");  break;  // 
                case 4:  ADoW[i]= dowWednesday; strcat(t,"We");  break;  // 
                case 5:  ADoW[i]= dowThursday;  strcat(t,"Th");  break;  // Четверг
                case 6:  ADoW[i]= dowFriday;    strcat(t,"Fr");  break;  // 
                case 7:  ADoW[i]= dowSaturday;  strcat(t,"Sa");  break;  // 
                } };
    AHour[0] = Clock.getHour(h12, PM);
    AMinute[0] = Clock.getMinute() + 2; if (AMinute[0]>=58) {AMinute[0]=0; AHour[0]++; };
    myAlarm[0] = Alarm.alarmRepeat(ADoW[0],AHour[0], AMinute[0],20,_AlarmOnFunc_);
    
   // Alarm.alarmRepeat(hour(), minute()+1, 20, _AlarmOnFunc_);
   // Alarm.timerOnce(20, MySignal);             // called once after 20 seconds 
    MyPrint_P(PSTR("WA START"),1,1);  
  //myPrint(itoa(AMinute[0],str_i,10),0,10);
    str_i[0]='\0';
    strcat(str_i,">>");
    strcat(str_i,t);  
    strcat(str_i,"->");
    
    itoa(AHour[0],t,10);
    strcat(str_i,t);  
    strcat(str_i,":"); 
    
    itoa(AMinute[0],t,10);    
    strcat(str_i,t);
    strcat(str_i," // "); 
    myPrint(str_i,0,10);
    tone(BEEP_PIN,3000,50);
    Alarm.delay(1000);
    
    AHour[1] = Clock.getHour(h12, PM);
    AMinute[1] = Clock.getMinute() + 8; if (AMinute[1]>=58) {AMinute[1]=5; AHour[1]++;};
//    myAlarm[1] = Alarm.alarmRepeat(ADoW[1],AHour[1],AMinute[1],20,_AlarmOnFunc_);  
    MyPrint_P(PSTR("ALARM2 STARTED "),0,1);  
    str_i[0]='\0';
    strcat(str_i," - > ");
    itoa(AHour[1],t,10);
    strcat(str_i,t);  
    strcat(str_i,":"); 
    itoa(AMinute[1],t,10);    
    strcat(str_i,t);
    strcat(str_i," // "); 
    myPrint(str_i,0,10);
 // myPrint(itoa(AHour[1],str_i,10),1,1);
    tone(BEEP_PIN,3000,50);
    Alarm.delay(1000);
 }
#endif

/* В параметре даем указатель на строку во ФЛЭШ ПАМЯТИ */
/* По данному указателю берем строку и запихиваем ее в строку STR_TMP */
/* Далее строку STR_TMP выводим на дисплей либо в порт */
void MyPrint_P (const char* str1, byte p, byte z) {   // p = 0-без времени/1-со временем  z = 0-полное время/1-короткое/2-частично
  char *str_tmp = new char[100];
  PutTimeStr(z); // пишим время в строку Time_str
  // Serial.println(Time_str);
  int i = 0;   
  while (pgm_read_byte(str1)!='\0')
    {
     str_tmp[i] = pgm_read_byte(str1);
     str1++;  
     i++;
    }
   str_tmp[i] = '\0';
   if (p==1){ str_tmp = strcat(str_tmp, Time_str); } // копируем время в текст сообщения   
   #if TP_OUT == 1    // to COM
     Serial.println(str_tmp);  
   #elif TP_OUT == 2  // to BT(SoftSerial)
     mySerial.println(str_tmp);  
   #elif TP_OUT == 3  // to LED
     read_line_str(str_tmp);     
   #else  
   #endif 
   delete [] str_tmp; // высвобождение памяти   
 }
 
void myPrint (const char* str1, byte p, byte z) {   // p = 0(без времени)/1(со временем) ||  z = 0-полное время/1-короткое/2-частично
  char *str_tmp = new char[100];
   PutTimeStr(z); // пишим время в строку Time_str
   // tone(BEEP_PIN,3000,50);
   // PutTimeStr_n(z);
   // Serial.println(Time_str);
   strcpy(str_tmp,str1);
   if (p==1){ str_tmp = strcat(str_tmp, Time_str); } // копируем время в текст сообщения   
   #if TP_OUT == 1    // to COM
     Serial.println(str_tmp);  
   #elif TP_OUT == 2  // to BT(SoftSerial)
     mySerial.println(str_tmp);  
   #elif TP_OUT == 3  // to LED
     read_line_str(str_tmp);     
   #else  
   #endif 
   delete [] str_tmp; // высвобождение памяти   
 }


