/*Sistema con almacenamiento permanente en EEPROM
 * Las tarjetas se guardan incluso al apagar el Arduino
 */
 
#include <Wire.h>
#include <Servo.h>
#include <LiquidCrystal_I2C.h>
#include <SPI.h>
#include <MFRC522.h>
#include <EEPROM.h>  // Para guardar en memoria permanente

// ================= CONFIGURACIÓN =================
#define SS_PIN 53
#define RST_PIN 5
#define SERVO_PIN 3

#define MAX_TARJETAS 10
#define UID_LENGTH 11  // "XX XX XX XX" = 11 caracteres

struct Tarjeta {
  char uid[UID_LENGTH + 1];  // +1 para el carácter nulo
  bool activa;
};

Tarjeta tarjetas[MAX_TARJETAS];
int numTarjetas = 0;
byte estadoPuerta = 0;

// ================= OBJETOS =================
Servo miServo;
LiquidCrystal_I2C lcd(0x27, 16, 2);
MFRC522 rfid(SS_PIN, RST_PIN);

// ================= SETUP =================
void setup() {
  Serial.begin(9600);
  Wire.begin();
  
  // Inicializar componentes
  lcd.init();
  lcd.backlight();
  miServo.attach(SERVO_PIN);
  miServo.write(0);
  
  SPI.begin();
  rfid.PCD_Init();
  
  // Cargar tarjetas de EEPROM
  cargarTarjetasEEPROM();
  
  // Mostrar mensaje inicial
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Bienvenido:");
  
  lcd.setCursor(0, 1);
  lcd.print("Tarjeta...?");
  
  Serial.println("=== MODO USUARIO ===");
  Serial.println("Comandos disponibles:");
  Serial.println("  A - Agregar tarjeta");
  Serial.println("  E - Eliminar tarjeta");
  Serial.println("  L - Listar tarjetas");
  Serial.println("  R - Reiniciar sistema");
}

// ================= LOOP =================
void loop() {
  // Verificar comandos por Serial
  if (Serial.available()) {
    char comando = Serial.read();
    procesarComando(comando);
  }
  
  // Verificar tarjeta RFID
  if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
    String uidLeido = obtenerUID();
    Serial.print("Tarjeta detectada: ");
    Serial.println(uidLeido);
    
    if (verificarTarjeta(uidLeido)) {
      // Tarjeta autorizada - cambiar estado puerta
      lcd.clear();
      if (estadoPuerta == 0) {
        miServo.write(180);
        lcd.print("Puerta ABIERTA");
        estadoPuerta = 1;
      } else {
        miServo.write(0);
        lcd.print("Puerta CERRADA");
        estadoPuerta = 0;
      }
      delay(2000);
    } else {
      // Tarjeta no autorizada
      lcd.clear();
      lcd.print("Acceso DENEGADO");
      delay(1500);
    }
    
    rfid.PICC_HaltA();
    rfid.PCD_StopCrypto1();
    
    // Restaurar mensaje inicial
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Bienvenido: ");
    
    lcd.setCursor(0, 1);
    lcd.print("Tarjeta...?");
  }
  
  delay(100);
}

// ================= FUNCIONES EEPROM =================

void cargarTarjetasEEPROM() {
  numTarjetas = EEPROM.read(0);  // Primer byte guarda la cantidad
  
  if (numTarjetas > MAX_TARJETAS) numTarjetas = 0;
  
  int direccion = 1;
  for (int i = 0; i < numTarjetas; i++) {
    for (int j = 0; j < UID_LENGTH; j++) {
      tarjetas[i].uid[j] = EEPROM.read(direccion++);
    }
    tarjetas[i].uid[UID_LENGTH] = '\0';
    tarjetas[i].activa = true;
  }
  
  Serial.print("Tarjetas cargadas: ");
  Serial.println(numTarjetas);
}

void guardarTarjetasEEPROM() {
  EEPROM.write(0, numTarjetas);
  
  int direccion = 1;
  for (int i = 0; i < numTarjetas; i++) {
    for (int j = 0; j < UID_LENGTH; j++) {
      EEPROM.write(direccion++, tarjetas[i].uid[j]);
    }
  }
  
  Serial.println("Tarjetas guardadas en EEPROM");
}

// ================= FUNCIONES TARJETAS =================

String obtenerUID() {
  String uid = "";
  for (byte i = 0; i < rfid.uid.size; i++) {
    if (rfid.uid.uidByte[i] < 0x10) uid += " 0";
    else uid += " ";
    uid += String(rfid.uid.uidByte[i], HEX);
  }
  uid.toUpperCase();
  return uid.substring(1);
}

bool verificarTarjeta(String uid) {
  for (int i = 0; i < numTarjetas; i++) {
    if (String(tarjetas[i].uid) == uid && tarjetas[i].activa) {
      return true;
    }
  }
  return false;
}

void agregarTarjeta(String uid) {
  if (numTarjetas >= MAX_TARJETAS) {
    Serial.println("Error: No hay espacio para más tarjetas");
    return;
  }
  
  // Verificar si ya existe
  for (int i = 0; i < numTarjetas; i++) {
    if (String(tarjetas[i].uid) == uid) {
      Serial.println("Error: Tarjeta ya registrada");
      return;
    }
  }
  
  // Agregar nueva tarjeta
  uid.toCharArray(tarjetas[numTarjetas].uid, UID_LENGTH + 1);
  tarjetas[numTarjetas].activa = true;
  numTarjetas++;
  
  guardarTarjetasEEPROM();
  
  Serial.print("Tarjeta agregada: ");
  Serial.println(uid);
  Serial.print("Total tarjetas: ");
  Serial.println(numTarjetas);
  delay(100);
}

void eliminarTarjeta(String uid) {
  for (int i = 0; i < numTarjetas; i++) {
    if (String(tarjetas[i].uid) == uid) {
      // Mover las tarjetas siguientes una posición atrás
      for (int j = i; j < numTarjetas - 1; j++) {
        tarjetas[j] = tarjetas[j + 1];
      }
      numTarjetas--;
      
      guardarTarjetasEEPROM();
      
      Serial.print("Tarjeta eliminada: ");
      Serial.println(uid);
      Serial.print("Total tarjetas: ");
      Serial.println(numTarjetas);
      delay(100);
      return;
    }
  }
  
  Serial.println("Error: Tarjeta no encontrada");
}

void listarTarjetas() {
  Serial.println("=== TARJETAS REGISTRADAS ===");
  if (numTarjetas == 0) {
    Serial.println("No hay tarjetas registradas");
  } else {
    for (int i = 0; i < numTarjetas; i++) {
      Serial.print(i + 1);
      Serial.print(": ");
      Serial.println(tarjetas[i].uid);
    }
  }
}

// ================= FUNCIONES COMANDOS =================

void procesarComando(char comando) {
  switch (comando) {
    case 'A':
    case 'a':
      modoAgregarTarjeta();
      break;
      
    case 'E':
    case 'e':
      modoEliminarTarjeta();
      break;
      
    case 'L':
    case 'l':
      listarTarjetas();
      break;
      
    case 'R':
    case 'r':
      reiniciarSistema();
      break;
      
    default:
      Serial.println("Comando no reconocido");
  }
}

void modoAgregarTarjeta() {
  Serial.println("=== MODO AGREGAR TARJETA ===");
  Serial.println("Acerca la tarjeta a registrar...");
  Serial.println("Presiona X para cancelar");
  
  lcd.clear();
  lcd.print("MODO: AGREGAR");
  lcd.setCursor(0, 1);
  lcd.print("Esperando...");
  
  while (true) {
    // Verificar cancelación
    if (Serial.available()) {
      if (Serial.read() == 'X' || Serial.read() == 'x') {
        Serial.println("Cancelado");
        break;
      }
    }
    
    // Verificar tarjeta
    if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
      String uid = obtenerUID();
      agregarTarjeta(uid);
      
      rfid.PICC_HaltA();
      rfid.PCD_StopCrypto1();
      break;
    }
    
    delay(100);
  }
  
  setup();  // Volver al estado inicial
}

void modoEliminarTarjeta() {
  Serial.println("=== MODO ELIMINAR TARJETA ===");
  Serial.println("Acerca la tarjeta a eliminar...");
  Serial.println("Presiona X para cancelar");
  
  lcd.clear();
  lcd.print("MODO: ELIMINAR");
  lcd.setCursor(0, 1);
  lcd.print("Esperando...");
  
  while (true) {
    if (Serial.available()) {
      if (Serial.read() == 'X' || Serial.read() == 'x') {
        Serial.println("Cancelado");
        break;
      }
    }
    
    if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
      String uid = obtenerUID();
      eliminarTarjeta(uid);
      
      rfid.PICC_HaltA();
      rfid.PCD_StopCrypto1();
      break;
    }
    
    delay(100);
  }
  
  setup();
}

void reiniciarSistema() {
  Serial.println("Reiniciando sistema...");
  numTarjetas = 0;
  guardarTarjetasEEPROM();
  Serial.println("Sistema reiniciado. Todas las tarjetas eliminadas.");
  setup();
}
