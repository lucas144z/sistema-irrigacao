#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// =====================================================
// CONFIGURAÇÃO DO DISPLAY OLED
// =====================================================

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// =====================================================
// PINOS
// =====================================================

const int analogPin = A0;     // Sensor de umidade
const int bombaPin = 4;       // Relé / bomba

// =====================================================
// CONFIGURAÇÃO DO RELÉ
// =====================================================
// Se seu relé for ativo em LOW, troque os valores:
//
// RELE_LIGA    = LOW
// RELE_DESLIGA = HIGH
// =====================================================

const int RELE_LIGA = HIGH;
const int RELE_DESLIGA = LOW;

// =====================================================
// CALIBRAÇÃO DO SENSOR
// =====================================================
//
// valorSeco = valor do sensor completamente seco
// valorAgua = valor do sensor completamente molhado
//
// Ajuste esses valores de acordo com seu sensor.
//
// =====================================================

const int valorSeco = 1023;
const int valorAgua = 300;

// =====================================================
// LIMITES DA BOMBA
// =====================================================
//
// A bomba liga quando chegar a 30% ou menos.
//
// Depois que ligar, ela continua funcionando até
// atingir 60%.
//
// Isso evita liga/desliga constante.
//
// =====================================================

const int limiteLiga = 30;
const int limiteDesliga = 60;

// =====================================================
// SEGURANÇA DA BOMBA
// =====================================================
//
// Tempo máximo que a bomba pode ficar ligada.
//
// 30 segundos = 30000 ms
//
// Se chegar nesse tempo sem o solo atingir 60%,
// a bomba será desligada e o sistema entrará em erro.
//
// =====================================================

const unsigned long tempoMaxBomba = 30000;

// =====================================================
// TEMPO MÍNIMO ENTRE ACIONAMENTOS
// =====================================================
//
// Depois que a bomba desliga, esperamos alguns segundos
// antes de permitir uma nova ativação.
//
// Isso ajuda a evitar acionamentos muito rápidos.
//
// =====================================================

const unsigned long tempoEsperaBomba = 5000;

// =====================================================
// VARIÁVEIS
// =====================================================

int analogValue = 0;
int percentage = 0;

bool bombaLigada = false;
bool erroBomba = false;

unsigned long inicioBomba = 0;
unsigned long ultimaParadaBomba = 0;

// =====================================================
// LEITURA DO SENSOR
// =====================================================
//
// Faz várias leituras e calcula a média.
//
// Isso reduz oscilações do sensor.
//
// =====================================================

int lerUmidade() {

  const int quantidadeLeituras = 10;

  long soma = 0;

  for (int i = 0; i < quantidadeLeituras; i++) {

    soma += analogRead(analogPin);

    delay(5);
  }

  return soma / quantidadeLeituras;
}

// =====================================================
// DESENHA A CARINHA
// =====================================================

void drawFace(int x, int y, int mood) {

  // Rosto
  display.drawCircle(
    x,
    y,
    20,
    SSD1306_WHITE
  );

  // Olhos
  display.fillCircle(
    x - 6,
    y - 6,
    4,
    SSD1306_WHITE
  );

  display.fillCircle(
    x + 6,
    y - 6,
    4,
    SSD1306_WHITE
  );

  // ---------------------------------------------------
  // FELIZ
  // ---------------------------------------------------

  if (mood == 1) {

    display.drawLine(
      x - 7,
      y + 8,
      x - 3,
      y + 10,
      SSD1306_WHITE
    );

    display.drawLine(
      x - 3,
      y + 10,
      x + 3,
      y + 10,
      SSD1306_WHITE
    );

    display.drawLine(
      x + 3,
      y + 10,
      x + 7,
      y + 8,
      SSD1306_WHITE
    );
  }

  // ---------------------------------------------------
  // NEUTRO
  // ---------------------------------------------------

  else if (mood == 0) {

    display.drawLine(
      x - 7,
      y + 10,
      x + 7,
      y + 10,
      SSD1306_WHITE
    );
  }

  // ---------------------------------------------------
  // TRISTE
  // ---------------------------------------------------

  else {

    display.drawLine(
      x - 7,
      y + 10,
      x - 3,
      y + 8,
      SSD1306_WHITE
    );

    display.drawLine(
      x - 3,
      y + 8,
      x + 3,
      y + 8,
      SSD1306_WHITE
    );

    display.drawLine(
      x + 3,
      y + 8,
      x + 7,
      y + 10,
      SSD1306_WHITE
    );
  }
}

// =====================================================
// DESENHA A BARRA DE UMIDADE
// =====================================================

void drawBarra(int umidade) {

  // Moldura da barra
  display.drawRect(
    10,
    55,
    108,
    8,
    SSD1306_WHITE
  );

  // Converte 0-100 para 0-104 pixels
  int largura = map(
    umidade,
    0,
    100,
    0,
    104
  );

  largura = constrain(
    largura,
    0,
    104
  );

  // Preenche a barra
  if (largura > 0) {

    display.fillRect(
      12,
      57,
      largura,
      4,
      SSD1306_WHITE
    );
  }
}

// =====================================================
// LIGA A BOMBA
// =====================================================

void ligarBomba() {

  bombaLigada = true;

  inicioBomba = millis();

  digitalWrite(
    bombaPin,
    RELE_LIGA
  );

  Serial.println(
    F("Solo SECO -> Bomba LIGADA")
  );
}

// =====================================================
// DESLIGA A BOMBA
// =====================================================

void desligarBomba() {

  bombaLigada = false;

  ultimaParadaBomba = millis();

  digitalWrite(
    bombaPin,
    RELE_DESLIGA
  );

  Serial.println(
    F("Bomba DESLIGADA")
  );
}

// =====================================================
// CONTROLE DA BOMBA
// =====================================================

void controlarBomba(int umidade) {

  // ===================================================
  // SISTEMA EM ERRO
  // ===================================================

  if (erroBomba) {

    // Só libera o sistema novamente quando o solo
    // atingir uma umidade segura.

    if (umidade >= limiteDesliga) {

      erroBomba = false;

      Serial.println(
        F("Umidade normalizada -> Erro LIMPO")
      );
    }

    return;
  }

  // ===================================================
  // SE A BOMBA ESTIVER LIGADA
  // ===================================================

  if (bombaLigada) {

    // -------------------------------------------------
    // Solo chegou à umidade desejada
    // -------------------------------------------------

    if (umidade >= limiteDesliga) {

      desligarBomba();

      Serial.println(
        F("Solo UMIDO -> Irrigacao concluida")
      );

      return;
    }

    // -------------------------------------------------
    // Proteção contra bomba ligada por muito tempo
    // -------------------------------------------------

    if (millis() - inicioBomba >= tempoMaxBomba) {

      desligarBomba();

      erroBomba = true;

      Serial.println(
        F("ERRO: tempo maximo da bomba atingido!")
      );

      Serial.println(
        F("Verifique agua, bomba, mangueira ou sensor.")
      );

      return;
    }
  }

  // ===================================================
  // SE A BOMBA ESTIVER DESLIGADA
  // ===================================================

  else {

    // -------------------------------------------------
    // Verifica se o solo está seco
    // -------------------------------------------------

    if (umidade <= limiteLiga) {

      // ------------------------------------------------
      // Verifica tempo mínimo desde o último acionamento
      // ------------------------------------------------

      if (
        millis() - ultimaParadaBomba >=
        tempoEsperaBomba
      ) {

        ligarBomba();
      }
    }
  }
}

// =====================================================
// TELA PRINCIPAL
// =====================================================

void mostrarTela(int umidade) {

  display.clearDisplay();

  // ===================================================
  // STATUS
  // ===================================================

  display.setTextSize(1);

  display.setCursor(
    0,
    0
  );

  display.print(
    F("Bomba: ")
  );

  if (erroBomba) {

    display.print(
      F("ERRO")
    );
  }

  else if (bombaLigada) {

    display.print(
      F("LIGADA")
    );
  }

  else {

    display.print(
      F("DESLIGADA")
    );
  }

  // ===================================================
  // PORCENTAGEM
  // ===================================================

  display.setTextSize(3);

  display.setCursor(
    8,
    18
  );

  display.print(
    umidade
  );

  display.print(
    F("%")
  );

  // ===================================================
  // CARINHA
  // ===================================================

  if (erroBomba) {

    // Carinha neutra durante erro
    drawFace(
      100,
      30,
      0
    );
  }

  else if (umidade >= limiteDesliga) {

    // Solo bem hidratado
    drawFace(
      100,
      30,
      1
    );
  }

  else if (umidade > limiteLiga) {

    // Umidade média
    drawFace(
      100,
      30,
      0
    );
  }

  else {

    // Solo seco
    drawFace(
      100,
      30,
      -1
    );
  }

  // ===================================================
  // BARRA
  // ===================================================

  drawBarra(
    umidade
  );

  // Envia para o OLED
  display.display();
}

// =====================================================
// MONITOR SERIAL
// =====================================================

void mostrarSerial() {

  Serial.print(
    F("Sensor: ")
  );

  Serial.print(
    analogValue
  );

  Serial.print(
    F(" | Umidade: ")
  );

  Serial.print(
    percentage
  );

  Serial.print(
    F("% | Bomba: ")
  );

  if (erroBomba) {

    Serial.println(
      F("ERRO")
    );
  }

  else if (bombaLigada) {

    Serial.println(
      F("LIGADA")
    );
  }

  else {

    Serial.println(
      F("DESLIGADA")
    );
  }
}

// =====================================================
// SETUP
// =====================================================

void setup() {

  Serial.begin(
    9600
  );

  // ===================================================
  // PINOS
  // ===================================================

  pinMode(
    analogPin,
    INPUT
  );

  pinMode(
    bombaPin,
    OUTPUT
  );

  // ===================================================
  // GARANTE BOMBA DESLIGADA AO INICIAR
  // ===================================================

  digitalWrite(
    bombaPin,
    RELE_DESLIGA
  );

  // ===================================================
  // OLED
  // ===================================================

  if (
    !display.begin(
      SSD1306_SWITCHCAPVCC,
      0x3C
    )
  ) {

    Serial.println(
      F("ERRO: OLED nao encontrado!")
    );

    while (true) {

      delay(10);
    }
  }

  // ===================================================
  // TELA DE INICIALIZAÇÃO
  // ===================================================

  display.clearDisplay();

  display.setTextColor(
    SSD1306_WHITE
  );

  display.setTextSize(1);

  display.setCursor(
    15,
    20
  );

  display.print(
    F("KitKraft")
  );

  display.setCursor(
    10,
    35
  );

  display.print(
    F("Soil Sensor v2")
  );

  display.display();

  delay(2000);

  // ===================================================
  // SERIAL
  // ===================================================

  Serial.println();
  Serial.println(
    F("================================")
  );

  Serial.println(
    F("     KITKRAFT SOIL SENSOR")
  );

  Serial.println(
    F("================================")
  );

  Serial.println(
    F("Sistema iniciado.")
  );

  Serial.println();
}

// =====================================================
// LOOP
// =====================================================

void loop() {

  // ===================================================
  // LEITURA DO SENSOR
  // ===================================================

  analogValue = lerUmidade();

  // ===================================================
  // CONVERSÃO PARA PORCENTAGEM
  // ===================================================

  percentage = map(
    analogValue,
    valorSeco,
    valorAgua,
    0,
    100
  );

  // Garante 0-100%
  percentage = constrain(
    percentage,
    0,
    100
  );

  // ===================================================
  // CONTROLE DA BOMBA
  // ===================================================

  controlarBomba(
    percentage
  );

  // ===================================================
  // OLED
  // ===================================================

  mostrarTela(
    percentage
  );

  // ===================================================
  // MONITOR SERIAL
  // ===================================================

  mostrarSerial();

  // ===================================================
  // ATUALIZAÇÃO
  // ===================================================

  delay(1000);
}
