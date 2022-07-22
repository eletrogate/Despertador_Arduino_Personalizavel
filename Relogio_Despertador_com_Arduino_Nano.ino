/***************************************************
  Relógio Despertador com Arduino Nano
  Criado em 04 de Setembro de 2021 por Michel Galvão
 ****************************************************/

// Inclusão das bibliotecas
#include <Wire.h>
#include "RTClib.h"
#include <Keypad.h>
#include <Keypad_I2C.h>
#include <LiquidCrystal_I2C.h>
#include <EEPROM.h>
#include "NotasMusicais.h"

// Definição Pinos
#define pinoBuzzer 9 // Pino do buzzer
#define pinoDesligaDespertador 2
#define pinoControleBacklightLCD 6 // Pino de controle de backlight do display LCD

//Definição Endereços EEPROM
#define enderecoHoraADespertar 0
#define enderecoMinutoADespertar 1
#define enderecoAtivaOuNaoDespertador 2
#define enderecoSomADespertar 3

//Definição Endereços I2C
#define enderecoRTC3231 0x68 // Endereço do RTC
#define enderecoDisplay 0x27 // Endereço do display LCD
#define enderecoTeclado 0x26 // Endereço do teclado matricial

// Configuração RTC
RTC_DS3231 rtc;

// Declaração de Variáveis
//// Variáveis de armazenamento de dados da EEPROM
int horaADespertar;
int minutoADespertar;
int deveDespertar;
int somADespertar;

//// Variáveis de controle do Despertador
bool estadoParaDesativacaoDespertador;
bool desperta = false;
int horaPosSoneca = 99;
int minutoPosSoneca = 99;

//// Variável de controle Menu LCD
bool indiceMenu = 0;

// Configuração LCD
LiquidCrystal_I2C lcd(0x27, 16, 2); // defina o endereço LCD para 0x27 para um display de 16 caracteres e 2 linhas

// Configuração Teclado Matricial I2C
const byte linhas = 4; // número de linhas no teclado
const byte colunas = 4; // número de colunas no teclado
char keys[linhas][colunas] = { // array que armazena as teclas (Mapeação de teclas)
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};
byte rowPins[linhas] = {0, 1, 2, 3}; // conecta-se aos pinos de linha do teclado
byte colPins[colunas] = {4, 5, 6, 7}; // conecta-se aos pinos de coluna do teclado
Keypad_I2C teclado(makeKeymap(keys), rowPins, colPins, linhas, colunas, enderecoTeclado, PCF8574 ); // teclado(mapa_de_teclas, nº_de_linhas_no_teclado, nº_de_colunas_no_teclado, endereço_I2C_do_teclado, tipo_de_Expansor_de_Porta);


void setup() {
  Serial.begin(9600);
  lcd.init(); // inicializar o lcd
  lcd.clear(); // Limpa a tela LCD
  lcd.backlight(); // Deixa  a luz de fundo do display LCD ligada

  lcd.setCursor(0, 0);
  lcd.print("Relogio         ");
  lcd.setCursor(0, 1);
  lcd.print("Despertador     ");
  delay(700);

  lcd.setCursor(0, 0);
  lcd.print("  --:--:-- --");
  /* simbolo grau (11011111[binário] = 223[decimal])*/ lcd.write(223); lcd.print("C ");
  lcd.setCursor(0, 1);
  lcd.print("  --/--/----    ");
  delay(500);

  pinMode(pinoBuzzer, OUTPUT); // Configura o pino do Buzzer como saída
  pinMode(pinoDesligaDespertador, INPUT); // Configura o pino do botão de desligar o Buzzer como saída
  digitalWrite(pinoBuzzer, LOW); // Deixa o Buzzer desligado
  Wire.begin(); // Inicializa a Comunicação I2C
  teclado.begin(makeKeymap(keys)); // Inicializa o Teclado Matricial I2C

  if (!rtc.begin()) { // Não foi possível encontrar o RTC
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("RTC ñ encontrado");
    while (!rtc.begin()) {
      lcd.setCursor(0, 0);
      lcd.print("Tentando conexao");
      lcd.setCursor(0, 1);
      lcd.print(" com RTC...     ");
      delay(1000);
    }
    lcd.clear();
  }

  if (rtc.lostPower()) { // O RTC perdeu energia, acerte a hora
    lcd.setCursor(0, 0);
    lcd.print("Acerte hora RTC ");
    delay(1500);
    configuraHorarioRTC(); // chama função para configurar a hora do RTC
  }

  lcd.clear(); // Limpa a tela LCD

  horaADespertar = EEPROM.read(enderecoHoraADespertar); // Obtém o valor da hora a despertar da memória EEPROM
  minutoADespertar = EEPROM.read(enderecoMinutoADespertar); // Obtém o valor do minuto a despertar da memória EEPROM
  deveDespertar = EEPROM.read(enderecoAtivaOuNaoDespertador); // Obtém o valor que indica se o despertador deve ou não despertar, da memória EEPROM
  somADespertar = EEPROM.read(enderecoSomADespertar); // Obtém o valor que indica o som que se deve tocar para despertar, da memória EEPROM

  estadoParaDesativacaoDespertador = !digitalRead(pinoDesligaDespertador); // Define o estado para parar de tocar o despertador: o inverso do estado lido do botão
}

void loop() {
  DateTime now = rtc.now(); // Faz a leitura de dados do RTC
  switch (indiceMenu) { // Testa a variável indiceMenu,
    case 0: // caso possua valor 0:
      telaPrincipal_mostraTempo(now.hour(),
                                now.minute(),
                                now.second(),
                                rtc.getTemperature(),
                                now.day(),
                                now.month(),
                                now.year()); // chama a função que exibe na tela do LCD o horário, a temperatura atual e a data atual
      break;
    case 1: // caso possua valor 1
      telaSecundaria_ExibeOpcoesMenu(); // chama a função que exibe na tela do LCD o menu de opções para configuração do horário e do despertador
      indiceMenu = 0; // após execução da função acima, define a variável de controle indiceMenu com 0 para retornar à visualização da tela principal
      break;
  }
  verificaSeDeveDespertar(now); // chama a função para verificar se deve-se acionar o despertador
  delay(10);
}
// função de toque do buzzer para feedback do aperto da tecla
void feedbackSomClique() {
  /* Função para executar um som no buzzer para sinalização
    de que alguma tecla foi pressionada*/
  digitalWrite(pinoBuzzer, HIGH);
  delay(25);
  digitalWrite(pinoBuzzer, LOW);
}
void telaPrincipal_mostraTempo(int hour,
                               int minute,
                               int second,
                               int temperature,
                               int day,
                               int month,
                               int year) {
  /* Função para exibir na tela do LCD o horário, a temperatura atual e a data atual.
    É necessário informar à função como parâmetros a hora, o minuto, o segundo, a temperatura, o dia, o mês e o ano.

    ----------------
    |< --:--:-- --ºC |
    |  --/--/----    |
    ----------------

  */

  lcd.setCursor(0, 0);
  lcd.print("< ");
  if (hour < 10) { // Caso a hora for menor do que 10 (somente um algarismo), é necessário exibir um caractere '0' para a devida formatação
    lcd.print("0");
    lcd.print(hour);
  } else {
    lcd.print(hour);
  }
  lcd.print(":");
  if (minute < 10) { // Caso o minuto for menor do que 10 (somente um algarismo), é necessário exibir um caractere '0' para a devida formatação
    lcd.print("0");
    lcd.print(minute);
  } else {
    lcd.print(minute);
  }
  lcd.print(":");
  if (second < 10) { // Caso o segundo for menor do que 10 (somente um algarismo), é necessário exibir um caractere '0' para a devida formatação
    lcd.print("0");
    lcd.print(second);
  } else {
    lcd.print(second);
  }
  lcd.print(" ");
  lcd.print(temperature);
  lcd.write(223); // simbolo grau
  lcd.print("C ");

  lcd.setCursor(0, 1);
  lcd.print("  ");
  if (day < 10) { // Caso o dia for menor do que 10 (somente um algarismo), é necessário exibir um caractere '0' para a devida formatação
    lcd.print("0");
    lcd.print(day);
  } else {
    lcd.print(day);
  }
  lcd.print("/");
  if (month < 10) { // Caso o mês for menor do que 10 (somente um algarismo), é necessário exibir um caractere '0' para a devida formatação
    lcd.print("0");
    lcd.print(month);
  } else {
    lcd.print(month);
  }
  lcd.print("/");
  lcd.print(year);
  lcd.print(" ");
  char tecla = teclado.getKey(); // leitura da tecla pressionada no Keypad
  if (tecla) { // se há alguma tecla,...
    feedbackSomClique(); // chama a função que executa o som de feedback de clique no teclado
  }
  switch (tecla) { // Testa a variável tecla,
      feedbackSomClique();
    case '*': // caso possua o valor '*', ...
      indiceMenu = !indiceMenu; // atribue à variável indiceMenu o valor oposto da mesma: ou seja, caso possua valor 1, irá virar 0 e caso possua valor 0 irá virar 1
      break;
  }
}

void telaSecundaria_ExibeOpcoesMenu() {
  /* Função para exibir na tela do LCD as opções da tela secundária.

    ----------------
    |> Data e Hora   |
    |  Despertador   |
    ----------------

  */

  const int indMinSubMenu = 0; // Variável para guardar o índice minímo do menu
  const int indMaxSubMenu = 1; // Variável para guardar o índice máximo do menu
  int indiceSubMenu = 0; // Variável para controle do menu atual selecionado
  bool controleWhile = true; // Variável para controle de saída do while
  while (controleWhile) { // Enquanto variável controleWhile continuar  com valor true, ...
    char tecla = teclado.getKey(); // obtém a tecla clicada
    if (tecla) { // se alguma tecla foi clicada, ...
      feedbackSomClique(); // faz som de feedback de clique
    }
    switch (tecla) { // Testa a variável tecla
      case 'A': // caso possua o valor 'A', ...
        indiceSubMenu--; // Decrementa o valor da variável
        break;
      case 'B': // caso possua o valor 'B', ...
        indiceSubMenu++; // Incrementa o valor da variável
        break;
      case '*': // caso possua o valor '*', ...
        controleWhile = false; // Define o valor da variável para false, para sair do while
        break;
      case '#': // caso possua o valor '#', ...
        if (indiceSubMenu == 0) { // Se a variavel possui o valor 0, ...
          configuraHorarioRTC(); // Chama a função para configurar o horário do RTC
          controleWhile = false; // Define o valor da variável para false, para sair do while
        } else if (indiceSubMenu == 1) { // Se não, se a variavel possui o valor 1, ...
          telaDespertador(); // Chama a função para execução do subMenu do Despertador
          controleWhile = false; // Define o valor da variável para false, para sair do while
        }
        break;

    }
    switch (indiceSubMenu) { // Testa a variável indiceSubMenu
      case 0: // Caso a variável posssua o valor 0, ...
        // Opção Data E Hora
        lcd.setCursor(0, 0);
        lcd.print("> Data E Hora   ");
        lcd.setCursor(0, 1);
        lcd.print("  Despertador   ");
        break;
      case 1: // Caso a variável posssua o valor 1, ...
        // Opção Despertador
        lcd.setCursor(0, 0);
        lcd.print("  Data E Hora   ");
        lcd.setCursor(0, 1);
        lcd.print("> Despertador   ");
        break;
    }
    if (indiceSubMenu > indMaxSubMenu) { // Se a variável de ìndice do Menu for maior que o número máximo de índice de menu, ...
      indiceSubMenu = indMinSubMenu; // Atribue o valor mínimo possível para a variável indiceSubMenu (menu volta à primeira opção)
    } else if (indiceSubMenu < indMinSubMenu) { // Se não, se a variável de ìndice do Menu for menor que o número mínimo de índice de menu, ...
      indiceSubMenu = indMaxSubMenu; // Atribue o valor máximo possível para a variável indiceSubMenu (menu volta à última opção)
    }
  }
}


void telaDespertador() {
  /* Função para exibir na tela do LCD as opções do menu Despertador.

    ----------------
    |> Conf. Horario |
    |  ON/OFF Desper.|
    |  Som Despertar |
    ----------------

  */

  const int indMinIndiceFuncao = 0; // Variável para guardar o índice minímo do menu
  const int indMaxIndiceFuncao = 2; // Variável para guardar o índice máximo do menu
  bool controleWhile = true; // Variável para controle de saída do while
  int indiceFuncao = 0; // Variável para controle da função atual selecionada
  while (controleWhile) { // Enquanto variável controleWhile continuar com valor true, ...
    switch (indiceFuncao) { // Testa a variável indiceFuncao
      case 0: // Caso a variável posssua o valor 0, ...
        // Configura Horario para Despertar
        lcd.setCursor(0, 0);
        lcd.print("> Conf. Horario ");
        lcd.setCursor(0, 1);
        lcd.print("  ON/OFF Desper.");
        break;
      case 1: // Caso a variável posssua o valor 1, ...
        //Ativação Despertador
        lcd.setCursor(0, 0);
        lcd.print("  Conf. Horario ");
        lcd.setCursor(0, 1);
        lcd.print("> ON/OFF Desper.");
        break;
      case 2: // Caso a variável posssua o valor 2, ...
        //Som Despertador
        lcd.setCursor(0, 0);
        lcd.print("  ON/OFF Desper.");
        lcd.setCursor(0, 1);
        lcd.print("> Som Despertar ");
        break;
    }
    char tecla = teclado.getKey(); // obtém a tecla clicada
    if (tecla) { // se alguma tecla foi clicada, ...
      feedbackSomClique(); // faz som de feedback de clique
    }

    switch (tecla) { // Testa a variável tecla
      case 'A': // caso possua o valor 'A', ...
        indiceFuncao--; // Decrementa o valor da variável
        break;
      case 'B': // caso possua o valor 'B', ...
        indiceFuncao++; // Incrementa o valor da variável
        break;
      case '*': // caso possua o valor '*', ...
        controleWhile = false; // Define o valor da variável para false, para sair do while
        break;
      case '#': // caso possua o valor '#', ...
        if (indiceFuncao == 0) { // Se a variavel possui o valor 0, ...
          configuraHorarioDespertador(); // Chama a função para configurar o horário de acionamento do Despertador
        } else if (indiceFuncao == 1) { // Se não, se a variavel possui o valor 1, ...
          configuraAtivacaoDespertador(); // Chama a função para ativar/desativar o Despertador
        } else if (indiceFuncao == 2) { // Se não, se a variavel possui o valor 2, ...
          configuraSomDespertador(); // Chama a função para configurar o som à ser acionado pelo Despertador
        }
        break;
    }
    if (indiceFuncao > indMaxIndiceFuncao) { // Se a variável de ìndice de Função for maior que o número máximo de índice de função, ...
      indiceFuncao = indMinIndiceFuncao; // Atribue o valor mínimo possível para a variável indiceFuncao (menu volta à primeira opção)
    } else if (indiceFuncao < indMinIndiceFuncao) { // Se a variável de ìndice de Função for menor que o número mínimo de índice de função, ...
      indiceFuncao = indMaxIndiceFuncao; // Atribue o valor mínimo possível para a variável indiceFuncao (menu volta à primeira opção)
    }
  } // Fim While
}

void configuraHorarioRTC() {
  /* Função para configurar a hora, o minuto, o segundo, o dia, o mês e o ano do RTC.

    ----------------
    |< --:--:--      |
    |  --/--/----    |
    ----------------

  */

  bool controleWhile = true; // Variável para controle de saída do while
  int indexVariavelASerAjustada = 0; // Variável para controle da variável à ser ajustada
  DateTime now = rtc.now(); // Faz a leitura de dados do RTC

  // Pré-Formatação pra exibição ao usuário
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("< ");
  lcd.print("HH"); lcd.print(":"); lcd.print("MM"); lcd.print(":"); lcd.print("SS");
  lcd.setCursor(2, 1);
  lcd.print("DD"); lcd.print("/"); lcd.print("MM"); lcd.print("/"); lcd.print("AAAA");
  delay(1500);

  // Variáveis para armazenar os valores à serem ajustados
  String hour;
  String minute;
  String second;
  String day;
  String month;
  String year;

  lcd.clear();

  if (now.hour() < 10) { // Se a hora atual for menor que 10, ...
    // Coloca a variável para com dois caracteres, sendo o primeiro o 0 (zero)
    hour = "0";
    hour += now.hour();
  } else {
    // Atribui à variável o tempo atual
    hour = now.hour();
  }

  if (now.minute() < 10) { // Se o minuto atual for menor que 10, ...
    // Coloca a variável para com dois caracteres, sendo o primeiro o 0 (zero)
    minute = "0";
    minute += now.minute();
  } else {
    // Atribui à variável o tempo atual
    minute = now.minute();
  }

  if (now.second() < 10) { // Se o segundo atual for menor que 10, ...
    // Coloca a variável para com dois caracteres, sendo o primeiro o 0 (zero)
    second = "0";
    second += now.second();
  } else {
    // Atribui à variável o tempo atual
    second = now.second();
  }

  if (now.day() < 10) { // Se o dia atual for menor que 10, ...
    // Coloca a variável para com dois caracteres, sendo o primeiro o 0 (zero)
    day = "0";
    day +=  now.day();
  } else {
    // Atribui à variável o tempo atual
    day =  now.day();
  }

  if (now.month() < 10) { // Se o mês atual for menor que 10, ...
    // Coloca a variável para com dois caracteres, sendo o primeiro o 0 (zero)
    month = "0";
    month +=  now.month();
  } else {
    // Atribui à variável o tempo atual
    month =  now.month();
  }

  year = now.year(); // Atribui à variável o tempo atual

  // Cria variáveis para armazenar os valores atuais, como forma de backup
  String backup_hour = hour;
  String backup_minute = minute;
  String backup_second = second;
  String backup_day = day;
  String backup_month = month;
  String backup_year = year;


  // Exibe ao usuário a formatação preenchida
  lcd.clear();
  lcd.print("< ");
  lcd.print(hour);
  lcd.print(":");
  lcd.print(minute);
  lcd.print(":");
  lcd.print(second);
  lcd.setCursor(2, 1);
  lcd.print(day);
  lcd.print("/");
  lcd.print(month);
  lcd.print("/");
  lcd.print(year);

  lcd.setCursor(2, 0);
  lcd.blink(); // Ativa o cursor piscante

  while (controleWhile) { // Enquanto a variável controleWhile seja true, ...
    char tecla = teclado.getKey();  // obtém a tecla clicada
    delay(100);
    if (isDigit(tecla)) { // Se a tecla pressionada for um caractere numérico (0-9), ...   Veja mais sobre isDigit() em: https://www.arduino.cc/reference/pt/language/functions/characters/isdigit/
      switch (indexVariavelASerAjustada) { // Testa a variável indexVariavelASerAjustada, ...
        case 0: // caso possua o valor 0, ...
          lcd.setCursor(2, 0);
          if (hour.length() >= 2) { // Se o tamanho da variável for maior ou igual à 2, ...
            hour = tecla; // Altera o valor da variável para o valor da tecla clicada
          } else if (hour.length() == 1) { // Se não, se o tamanho da variável for igual à 1, ...
            hour += tecla; // Adiciona à variável o valor da tecla clicada
          }
          if (hour.length() >= 2) { // Se o tamanho da variável for maior ou igual à 2, ...
            String aConverterINT = String(hour); // Cria uma variável para armazenar o valor da variável hour
            if (aConverterINT.toInt() < 24) { // Se o resultado da conversão da variável for menor que 24 (valor máximo que a hora pode assumir)
              hour += ":"; // Adiciona a variável o caractere ':'
              indexVariavelASerAjustada++; // Incrementa a variável
            } else { // Se não, ...
              hour = backup_hour; // Atribui o valor de backup à variável
              lcd.print(hour);
              lcd.setCursor(2, 0);
              break;
            }
          }
          lcd.print(hour);

          break;
        case 1: // caso possua o valor 1, ...
          lcd.setCursor(5, 0);
          if (minute.length() >= 2) { // Se o tamanho da variável for maior ou igual à 2, ...
            minute = tecla; // Altera o valor da variável para o valor da tecla clicada
          } else if (minute.length() == 1) { // Se não, se o tamanho da variável for igual à 1, ...
            minute += tecla; // Adiciona à variável o valor da tecla clicada
          }
          if (minute.length() >= 2) { // Se o tamanho da variável for maior ou igual à 2, ...
            String aConverterINT = String(minute); // Cria uma variável para armazenar o valor da variável hour
            if (aConverterINT.toInt() < 60) { // Se o resultado da conversão da variável for menor que 60 (valor máximo que o minuto pode assumir)
              minute += ":"; // Adiciona a variável o caractere ':'
              indexVariavelASerAjustada++; // Incrementa a variável
            } else { // Se não, ...
              minute = backup_minute; // Atribui o valor de backup à variável
              lcd.print(minute);
              lcd.setCursor(5, 0);
              break;
            }
          }
          lcd.print(minute);

          break;
        case 2: // caso possua o valor 2, ...
          lcd.setCursor(8, 0);
          if (second.length() >= 2) { // Se o tamanho da variável for maior ou igual à 2, ...
            second = tecla; // Altera o valor da variável para o valor da tecla clicada
          } else if (second.length() == 1) { // Se não, se o tamanho da variável for igual à 1, ...
            second += tecla; // Adiciona à variável o valor da tecla clicada
          }

          if (second.length() >= 2) { // Se o tamanho da variável for maior ou igual à 2, ...
            String aConverterINT = String(second);
            if (aConverterINT.toInt() < 60) {
              indexVariavelASerAjustada++; // Incrementa a variável
              lcd.print(second);
              lcd.setCursor(2, 1);
              break;
            } else {
              second = backup_second; // Atribui o valor de backup à variável
              lcd.print(second);
              lcd.setCursor(8, 0);
              break;
            }
          }
          lcd.print(second);

          break;
        case 3: // caso possua o valor 3, ...
          lcd.setCursor(2, 1);
          if (day.length() >= 2) {
            day = tecla; // Altera o valor da variável para o valor da tecla clicada
          } else if (day.length() == 1) {
            day += tecla; // Adiciona à variável o valor da tecla clicada
          }

          if (day.length() >= 2) {
            String aConverterINT = String(day);
            if (aConverterINT.toInt() > 0 && aConverterINT.toInt() <= 31) { // Se o resultado da conversão da variável for maior que 0 e o resultado da conversão da variável for menor que 31
              day += "/";
              indexVariavelASerAjustada++; // Incrementa a variável
            } else {
              day = backup_day; // Atribui o valor de backup à variável
              lcd.print(day);
              lcd.setCursor(2, 1);
              break;
            }
          }
          lcd.print(day);

          break;
        case 4: // caso possua o valor 4, ...
          lcd.setCursor(5, 1);
          if (month.length() >= 2) {
            month = tecla;
          } else if (month.length() == 1) {
            month += tecla;
          }
          if (month.length() >= 2) {
            String aConverterINT = String(month);
            if (aConverterINT.toInt() > 0 && aConverterINT.toInt() <= 12) { // Se o resultado da conversão da variável for maior que 0 e o resultado da conversão da variável for menor que 12
              month += "/";
              indexVariavelASerAjustada++;
            } else {
              month = backup_month;
              lcd.print(month);
              lcd.setCursor(5, 1);
              break;
            }
          }
          lcd.print(month);
          break;
        case 5: // caso possua o valor 5, ...
          lcd.setCursor(8, 1);
          if (year.length() >= 4) {
            year = tecla;
          } else if (year.length() <= 3) {
            year += tecla;
          }

          if (year.length() >= 4) {
            String aConverterINT = String(year);
            if (aConverterINT.toInt() > 2000 && aConverterINT.toInt() <= 2099) { // Se o resultado da conversão da variável for maior que 2000 e o resultado da conversão da variável for menor que 2099
              lcd.print(year);
              lcd.setCursor(2, 0);
              indexVariavelASerAjustada++;
              break;
            } else {
              year = backup_year;
              lcd.print(year);
              lcd.setCursor(8, 1);
              break;
            }
          }
          lcd.print(year);
          break;
      }

    }

    if (indexVariavelASerAjustada > 5) { // Se a variável de ìndice da variável for maior que 5, ...
      indexVariavelASerAjustada = 0; // Atribue o valor mínimo possível (0) para a variável indexVariavelASerAjustada (menu volta à primeira opção)
    }
    if (indexVariavelASerAjustada < 0) { // Se não, se a variável de ìndice da variável for menor que 0, ...
      indexVariavelASerAjustada = 5; // Atribue o valor máximo possível (5) para a variável indexVariavelASerAjustada (menu volta à última opção)
    }

    if (tecla) { // se alguma tecla foi clicada, ...
      feedbackSomClique(); // faz som de feedback de clique
    }
    switch (tecla) { // Testa a variável tecla
      case '*': // caso possua o valor '*', ...
        controleWhile = false; // Define o valor da variável para false, para sair do while
        break;
      case '#': // caso possua o valor '#', ...
        lcd.clear();
        lcd.setCursor(0, 0);

        //Cria variáveis para armazenar os valores das entradas do usuário
        String entrada_year = year;
        String entrada_month = month;
        String entrada_day = day;
        String entrada_hour = hour;
        String entrada_minute = minute;
        String entrada_second = second;

        //Remove das variáveis necessárias os caracteres '/' ou ':'
        entrada_month.replace("/", "");
        entrada_day.replace("/", "");
        entrada_hour.replace(":", "");
        entrada_minute.replace(":", "");

        // Veja mais sobre toInt() em: https://www.arduino.cc/reference/en/language/variables/data-types/string/functions/toint/
        rtc.adjust(DateTime(entrada_year.toInt(),
                            entrada_month.toInt(),
                            entrada_day.toInt(),
                            entrada_hour.toInt(),
                            entrada_minute.toInt(),
                            entrada_second.toInt())); // Método utilizado para ajustar o horário do RTC. Parâmetros: (ano, mês, dia, hora, minuto, segundo)
        lcd.print("Ajuste Horario  ");
        lcd.setCursor(0, 1);
        lcd.print("concluido!      ");
        delay(2000);
        controleWhile = false; // Define o valor da variável para false, para sair do while

        break;
    }

  }
  lcd.noBlink(); //Desativa o cursor piscante do LCD
}

void configuraHorarioDespertador() {
  /* Função para configurar o horário à ativar o despertador.

    ----------------
    |<     --:--     |
    |                |
    ----------------

  */

  bool controleWhile = true; // Variável para controle de saída do while
  int indexVariavelASerAjustada = 0; // Variável para controle da variável à ser ajustada

  // Variáveis para armazenar os valores à serem ajustados
  String hour;
  String minute;


  if (horaADespertar < 10) { // Se a hora a despertar for menor que 10, ...
    // Coloca a variável para com dois caracteres, sendo o primeiro o 0 (zero)
    hour = "0";
    hour += horaADespertar;
  } else {
    // Atribui à variável a hora a despertar
    hour = horaADespertar;
  }

  if (minutoADespertar < 10) { // Se o minuto a despertar for menor que 10, ...
    // Coloca a variável para com dois caracteres, sendo o primeiro o 0 (zero)
    minute = "0";
    minute += minutoADespertar;
  } else {
    // Atribui à variável o minuto a despertar
    minute = minutoADespertar;
  }

  // Cria variáveis para armazenar os valores hour e minute, como forma de backup
  String backup_hour = hour;
  String backup_minute = minute;

  // Formatação preenchida para visuaização do usuário
  lcd.clear();
  lcd.print("<     ");
  lcd.print(hour);
  lcd.print(":");
  lcd.print(minute);

  lcd.setCursor(6, 0);
  lcd.blink(); // Ativa o cursor piscante do LCD
  while (controleWhile) { // Enquanto variável controleWhile continuar com valor true, ...
    char tecla = teclado.getKey(); // obtém a tecla clicada
    delay(100);
    if (isDigit(tecla)) {
      switch (indexVariavelASerAjustada) {
        case 0:
          lcd.setCursor(6, 0);
          if (hour.length() >= 2) {
            hour = tecla;
          } else if (hour.length() == 1) {
            hour += tecla;
          }
          if (hour.length() >= 2) {
            String aConverterINT = String(hour);
            if (aConverterINT.toInt() < 24) {
              hour += ":";
              indexVariavelASerAjustada++;
            } else {
              hour = backup_hour;
              lcd.print(hour);
              lcd.setCursor(6, 0);
              break;
            }
          }
          lcd.print(hour);

          break;
        case 1:
          lcd.setCursor(9, 0);
          if (minute.length() >= 2) {
            minute = tecla;
          } else if (minute.length() == 1) {
            minute += tecla;
          }
          lcd.print(minute);
          if (minute.length() >= 2) {
            String aConverterINT = String(minute);
            if (aConverterINT.toInt() < 60) {
              indexVariavelASerAjustada++;
              lcd.setCursor(6, 0);
            } else {
              minute = backup_minute;
              lcd.setCursor(9, 0);
              lcd.print(minute);
              lcd.setCursor(9, 0);
              break;
            }
          }


          break;


      }

    }
    if (indexVariavelASerAjustada > 1) {
      indexVariavelASerAjustada = 0;
    }
    if (indexVariavelASerAjustada < 0) {
      indexVariavelASerAjustada = 1;
    }

    if (tecla) {
      feedbackSomClique();
    }
    switch (tecla) {
      case '*':
        controleWhile = false;
        break;
      case '#':
        lcd.clear();
        lcd.setCursor(0, 0);

        String entrada_hour = hour;
        String entrada_minute = minute;

        entrada_hour.replace(":", "");

        lcd.print("Despertador     ");
        lcd.setCursor(0, 1);
        lcd.print("concluido!      ");

        horaADespertar = entrada_hour.toInt(); // Atribue o valor de entrada_hour à horaADespertar
        minutoADespertar = entrada_minute.toInt(); // Atribue o valor de entrada_minute à minutoADespertar
        EEPROM.update(enderecoHoraADespertar, horaADespertar); // Escreva o valor da hora a despertar na memória EEPROM endereçado em enderecoHoraADespertar. 
        //O valor é escrito apenas se for diferente do que já foi salvo no mesmo endereço.
        delay(100);
        EEPROM.update(enderecoMinutoADespertar, minutoADespertar); // Escreva o valor do minuto a despertar na memória EEPROM endereçado em 
        //enderecoMinutoADespertar. O valor é escrito apenas se for diferente do que já foi salvo no mesmo endereço.
        delay(2000);
        controleWhile = false;
        break;
    }

  }
  lcd.noBlink();
}

void configuraSomDespertador() {
  /* Função para configurar o som à ser acionado quando despertar.

     ----------------
    |>Ringtone Nokia |
    | Ringt. Opening |
    | Ringt. Classico|
    | Ring. Beep Once|
     ----------------

  */

  bool controleWhile = true;
  int indiceConsulta = somADespertar;
  lcd.clear();
  switch (indiceConsulta) {
    case 0:
      lcd.setCursor(0, 0);
      lcd.print(">Ringtone Nokia ");
      lcd.setCursor(0, 1);
      lcd.print(" Ringt. Opening ");
      break;
    case 1:
      lcd.setCursor(0, 0);
      lcd.print(" Ringtone Nokia ");
      lcd.setCursor(0, 1);
      lcd.print(">Ringt. Opening ");
      break;
    case 2:
      lcd.setCursor(0, 0);
      lcd.print(">Ringt. Classico");
      lcd.setCursor(0, 1);
      lcd.print(" Ring. Beep Once");
      break;
    case 3:
      lcd.setCursor(0, 0);
      lcd.print(" Ringt. Classico");
      lcd.setCursor(0, 1);
      lcd.print(">Ring. Beep Once");
      break;
  }

  while (controleWhile) {
    char tecla = teclado.getKey();
    delay(100);
    if (tecla) {
      feedbackSomClique();
    }
    switch (tecla) {
      case '*':
        controleWhile = false;
        break;
      case '#':
        lcd.clear();
        lcd.setCursor(0, 0);
        somADespertar = indiceConsulta;
        EEPROM.update(enderecoSomADespertar, somADespertar);
        lcd.print("Configuracao    ");
        lcd.setCursor(0, 1);
        lcd.print("Salva!          ");
        delay(1200);
        controleWhile = false;
        break;
      case 'A':
        indiceConsulta--;
        break;
      case 'B':
        indiceConsulta++;
        break;
    }
    if (indiceConsulta > 3) {
      indiceConsulta = 0;
    } else if (indiceConsulta < 0) {
      indiceConsulta = 3;
    }

    switch (indiceConsulta) {
      case 0:
        lcd.setCursor(0, 0);
        lcd.print(">Ringtone Nokia ");
        lcd.setCursor(0, 1);
        lcd.print(" Ringt. Opening ");
        break;
      case 1:
        lcd.setCursor(0, 0);
        lcd.print(" Ringtone Nokia ");
        lcd.setCursor(0, 1);
        lcd.print(">Ringt. Opening ");
        break;
      case 2:
        lcd.setCursor(0, 0);
        lcd.print(">Ringt. Classico");
        lcd.setCursor(0, 1);
        lcd.print(" Ring. Beep Once");
        break;
      case 3:
        lcd.setCursor(0, 0);
        lcd.print(" Ringt. Classico");
        lcd.setCursor(0, 1);
        lcd.print(">Ring. Beep Once");
        break;

    }


    if (tecla && tecla != '*' && tecla != '#') { // Se foi pressionada alguma tecla e a tela pressionada for diferente de '*' e '#', ...
      switch (indiceConsulta) { // Testa a variável indiceConsulta
        case 0: // Caso o valor da variável for 0, ...
          despertaSom_RingtoneNokia(); // aciona o som Ringtone Nokia
          break;
        case 1: // Caso o valor da variável for 1, ...
          despertaSom_RingtoneOpening(); // aciona o som Ringtone Opening
          break;
        case 2: // Caso o valor da variável for 2, ...
          despertaSom_RingtoneClassico(); // aciona o som Ringtone Classico
          break;
        case 3: // Caso o valor da variável for 3, ...
          despertaSom_RingtoneBeepOnce(); // aciona o som Ringtone Beep Once
          break;
      }

    }
  }
}

void configuraAtivacaoDespertador() {
  /* Função para configurar se o despertador deve ou não despertar.
  */

  bool controleWhile = true;
  int indiceConsulta = deveDespertar;

  // Pré-formatação para visualização do usuário
  lcd.clear();
  if (deveDespertar == 1) {
    lcd.setCursor(0, 0);
    lcd.print("< *LIGADO       ");
    lcd.setCursor(0, 1);
    lcd.print("   DESLIGADO    ");
    lcd.setCursor(2, 0);
  } else if (deveDespertar == 0) {
    lcd.setCursor(0, 0);
    lcd.print("<  LIGADO       ");
    lcd.setCursor(0, 1);
    lcd.print("  *DESLIGADO    ");
    lcd.setCursor(2, 1);
  } else {
    lcd.setCursor(0, 0);
    lcd.print("<  LIGADO       ");
    lcd.setCursor(0, 1);
    lcd.print("   DESLIGADO    ");
    lcd.setCursor(2, 0);
  }

  lcd.blink();
  while (controleWhile) {
    char tecla = teclado.getKey();
    delay(100);

    if (indiceConsulta == 0) {
      lcd.setCursor(2, 1);
    } else if (indiceConsulta == 1) {
      lcd.setCursor(2, 0);
    }
    if (tecla) {
      feedbackSomClique();
    }
    if (indiceConsulta > 1) {
      indiceConsulta = 0;
    } else if (indiceConsulta < 0) {
      indiceConsulta = 1;
    }

    switch (tecla) {
      case '*':
        controleWhile = false;
        break;
      case '#':
        lcd.clear();
        lcd.setCursor(0, 0);
        deveDespertar = indiceConsulta; //  Atribue à variável deveDespertar o valor de indiceConsulta
        EEPROM.update(enderecoAtivaOuNaoDespertador, deveDespertar); // Salva na memória EEPROM o valor. Mesmo após a queda de energia, o valor irá permanecer na memória
        lcd.print("Configuracao    ");
        lcd.setCursor(0, 1);
        lcd.print("Salva!          ");
        delay(1200);
        controleWhile = false;
        break;
      case 'A':
        indiceConsulta++;
        break;
      case 'B':
        indiceConsulta--;
        break;
    }

  }
  lcd.noBlink();
}

void verificaSeDeveDespertar(DateTime agora) {
  /* Função para verificação de ativação do despertador.
      Tem com parâmetro um objeto DateTime contendo a hora, o minuto, o segundo, o dia, o mês e o ano atuais.
  */
  if (deveDespertar == 1) {
    // caso é verificado que o despertador está permitido à despertar, ...
    if (agora.hour() == horaADespertar && agora.minute() == minutoADespertar && agora.second() == 0) {
      // caso a hora atual for igual a hora a despertar e
      // * o minuto atual for igual ao minuto a despertar e
      // * o segundo atual for igual à 0, ...
      estadoParaDesativacaoDespertador = !digitalRead(pinoDesligaDespertador); // Atribui à variável de desativação do despertador o valor inverso da leitura digital do pino conectado ao botão
      desperta = true; // Autoriza o despertador à ser acionado
    }
  }


  if (desperta == true) { // Se o despertador estiver autorizado à despertar, ...
    lcd.setCursor(0, 0);
    lcd.print("Alarme das      ");
    lcd.setCursor(0, 1);
    if (horaADespertar < 10) {
      lcd.print("0");
    }
    lcd.print(horaADespertar);
    lcd.print(":");
    if (minutoADespertar < 10) {
      lcd.print("0");
    }
    lcd.print(minutoADespertar);
    lcd.print("           ");

    // Executa o som do Despertador de acordo com o que o usuário definiu
    switch (somADespertar) {
      case 0:
        despertaSom_RingtoneNokia();
        break;
      case 1:
        despertaSom_RingtoneOpening();
        break;
      case 2:
        despertaSom_RingtoneClassico();
        break;
      case 3:
        despertaSom_RingtoneBeepOnce();
        break;
    }

  }
}

void despertaSom_RingtoneNokia() {
  /*Função para executar o som Ringtone Nokia
  */

  int melodia[] = { // matriz para armazenar as notas à serem tocadas em sequência
    NOTE_E5, NOTE_D5, NOTE_FS4, NOTE_GS4, NOTE_CS5, NOTE_B4, NOTE_D4, NOTE_E4,
    NOTE_B4, NOTE_A4, NOTE_CS4, NOTE_E4, NOTE_A4, NO_SOUND
  };
  // tempoNotas: Semibreve:1; Mínima:2; Semínima: 4; Colcheia:8; Semicolcheia:16; Fusa:32; Semifusa:64;
  int tempoNotas[] = { // matriz para armazenar o tempo de cada armazenada na matriz da melodia
    8, 8, 4, 4, 8, 8, 4, 4,
    8, 8, 4, 4, 1, 4
  };
  int compasso = 800; // Tempo do compasso: quanto maior mais demorado, e quanto bais baixo mais rápido
  const int quantidadeNotas = sizeof(melodia) / sizeof(melodia[0]); // armazena a quantidade de notas armazenadas na matriz melodia

  for (int Nota = 0; Nota < quantidadeNotas; Nota++) {
    if (digitalRead(pinoDesligaDespertador) == estadoParaDesativacaoDespertador) { // se a leitura digital do pino conectado ao botão de desativação for igual ao estado estabelecido para desativação, ...
      delay(50); // delay para debounce do botão
      estadoParaDesativacaoDespertador = !digitalRead(pinoDesligaDespertador); // atribue à variável estadoParaDesativacaoDespertador o valor inverso da leitura digital do pino do botão
      desperta = false; // desliga despertador
    }
    int tempo = compasso / tempoNotas[Nota]; //Tempo = compasso dividido pela indicação da matriz tempoNotas.
    tone(pinoBuzzer, melodia[Nota], tempo); //Toca a nota indicada pela matriz melodia durante o tempo.
    // Para distinguir as notas adicionamos um tempo entre elas
    delay(tempo * 1.2);
  }

}
void despertaSom_RingtoneOpening() {
  /*Função para executar o som Ringtone Opening
  */

  int melodia[] = { // matriz para armazenar as notas à serem tocadas em sequência
    NOTE_C5, NOTE_AS4, NOTE_G4, NOTE_C5, NOTE_F4, NOTE_C5, NOTE_AS4, NOTE_C5,
    NOTE_F4, NO_SOUND, NO_SOUND, NOTE_G4, NOTE_G4, NOTE_AS4, NOTE_C5, NO_SOUND
  };

  // tempoNotas: Semibreve:1; Mínima:2; Semínima: 4; Colcheia:8; Semicolcheia:16; Fusa:32; Semifusa:64;
  int tempoNotas[] = { // matriz para armazenar o tempo de cada armazenada na matriz da melodia
    16, 16, 8, 8, 8, 8, 8, 8,
    8, 4, 8, 4, 8, 8, 8, 64
  };

  int compasso = 1400; // Tempo do compasso: quanto maior mais demorado, e quanto bais baixo mais rápido

  const int quantidadeNotas = sizeof(melodia) / sizeof(melodia[0]); // armazena a quantidade de notas armazenadas na matriz melodia
  for (int Nota = 0; Nota < quantidadeNotas; Nota++) {//o número 80 indica quantas notas tem a nossa matriz.
    if (digitalRead(pinoDesligaDespertador) == estadoParaDesativacaoDespertador) { // se a leitura digital do pino conectado ao botão de desativação for igual ao estado estabelecido para desativação, ...
      delay(50); // delay para debounce do botão
      estadoParaDesativacaoDespertador = !digitalRead(pinoDesligaDespertador); // atribue à variável estadoParaDesativacaoDespertador o valor inverso da leitura digital do pino do botão
      desperta = false; // desliga despertador
    }
    int tempo = compasso / tempoNotas[Nota]; //Tempo = compasso dividido pela indicação da matriz tempoNotas.
    tone(pinoBuzzer, melodia[Nota], tempo); //Toca a nota indicada pela matriz melodia durante o tempo.
    // Para distinguir as notas adicionamos um tempo entre elas
    delay(tempo * 1.2);
  }
}
void despertaSom_RingtoneClassico() {
  /*Função para executar o som Ringtone Classico
  */

  int melodia[] = { // matriz para armazenar as notas à serem tocadas em sequência
    NOTE_B6, NOTE_B6, NOTE_B6, NOTE_B6, NO_SOUND, NOTE_B6, NOTE_B6, NOTE_B6, NOTE_B6, NO_SOUND
  };


  // tempoNotas: Semibreve:1; Mínima:2; Semínima: 4; Colcheia:8; Semicolcheia:16; Fusa:32; Semifusa:64;
  int tempoNotas[] = { // matriz para armazenar o tempo de cada armazenada na matriz da melodia
    8, 8, 8, 8, 2, 8, 8, 8, 8, 2
  };

  int compasso = 1000; // Tempo do compasso: quanto maior mais demorado, e quanto bais baixo mais rápido

  const int quantidadeNotas = sizeof(melodia) / sizeof(melodia[0]); // armazena a quantidade de notas armazenadas na matriz melodia

  for (int Nota = 0; Nota < quantidadeNotas; Nota++) {//o número 80 indica quantas notas tem a nossa matriz.
    if (digitalRead(pinoDesligaDespertador) == estadoParaDesativacaoDespertador) { // se a leitura digital do pino conectado ao botão de desativação for igual ao estado estabelecido para desativação, ...
      delay(50); // delay para debounce do botão
      estadoParaDesativacaoDespertador = !digitalRead(pinoDesligaDespertador); // atribue à variável estadoParaDesativacaoDespertador o valor inverso da leitura digital do pino do botão
      desperta = false; // desliga despertador
    }
    int tempo = compasso / tempoNotas[Nota]; //Tempo = compasso dividido pela indicação da matriz tempoNotas.
    tone(pinoBuzzer, melodia[Nota], tempo); //Toca a nota indicada pela matriz melodia durante o tempo.
    // Para distinguir as notas adicionamos um tempo entre elas
    delay(tempo * 1.2);
  }
}
void despertaSom_RingtoneBeepOnce() {
  /*Função para executar o som Ringtone Beep Once
  */

  int melodia[] = { // matriz para armazenar as notas à serem tocadas em sequência
    NOTE_F5, NO_SOUND
  };

  // tempoNotas: Semibreve:1; Mínima:2; Semínima: 4; Colcheia:8; Semicolcheia:16; Fusa:32; Semifusa:64;
  int tempoNotas[] = {
    1, 2
  };

  int compasso = 300; // Tempo do compasso: quanto maior mais demorado, e quanto bais baixo mais rápido

  const int quantidadeNotas = sizeof(melodia) / sizeof(melodia[0]); // armazena a quantidade de notas armazenadas na matriz melodia
  for (int Nota = 0; Nota < quantidadeNotas; Nota++) {//o número 80 indica quantas notas tem a nossa matriz.
    if (digitalRead(pinoDesligaDespertador) == estadoParaDesativacaoDespertador) { // se a leitura digital do pino conectado ao botão de desativação for igual ao estado estabelecido para desativação, ...
      delay(50); // delay para debounce do botão
      estadoParaDesativacaoDespertador = !digitalRead(pinoDesligaDespertador); // atribue à variável estadoParaDesativacaoDespertador o valor inverso da leitura digital do pino do botão
      desperta = false; // desliga despertador
    }
    int tempo = compasso / tempoNotas[Nota]; //Tempo = compasso dividido pela indicação da matriz tempoNotas.
    tone(pinoBuzzer, melodia[Nota], tempo); //Toca a nota indicada pela matriz melodia durante o tempo.
    // Para distinguir as notas adicionamos um tempo entre elas
    delay(tempo * 1.2);
  }
}
