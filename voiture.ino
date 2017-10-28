// Pour le lecteur mp3
#include <SPI.h>
#include <Adafruit_VS1053.h>
#include <SD.h>

// Pour les moteurs pas à pas
#include <Servo.h>

// Pour les moteurs à courant continue
#include <AFMotor.h>

// Vitesse minimum pour les moteurs (en dessous de cette vitesse, ils ne tournent pas)
#define MIN_SPEED 40
// Valeur médiane des joysticks
#define MID_VALUE 127
// Valeur pour virage sérré à gauche
#define LEFT_TURN_THRESHOLD 20
// Valeur pour virage sérré à droite
#define RIGHT_TURN_THRESHOLD 255-LEFT_TURN_THRESHOLD

// Définition de borches à utiliser pour le shield mp3 (VS1053)
#define CLK 52       // Horloge SPI, partagée avec la carte sd
#define MISO 50      // Entrée de données, du sheild VS1053/carte SD
#define MOSI 51      // Sorties de données, vers le sheild VS1053/carte SD

#define BREAKOUT_RESET  22      // Broche de reset du shield VS1053 (sortie)
#define SHIELD_CS       23      // Broche "Chip Select" du shield VS1053 (sortie)
#define SHIELD_DCS      24      // Broche "Data/Command Select" du shield VS1053 (sortie)
#define CARDCS          25      // Broche "Card Chip Select" (sortie)
#define DREQ            21      // Broche "Data request" du shield VS1053 (Borche d'interruption)

// Création du player audio
Adafruit_VS1053_FilePlayer musicPlayer = Adafruit_VS1053_FilePlayer(BREAKOUT_RESET, SHIELD_CS, SHIELD_DCS, DREQ, CARDCS);
  
// Variables globales
// Etat du relai 1
bool relay1 = false;
// Etat du relai 2
bool relay2 = false;
// Position du servomoteur 1
int servo1pos = 10;
// Position du servomoteur 2
int servo2pos = 10;

// Variables pour la gestion des moteurs à courant continue (roue gauche et roue droite)
// Position courante du joystick avant / arrière
int currentX = MID_VALUE;
// Position courante du joystick gauche / droite
int currentY = MID_VALUE;
// Vitesse courante
int currentSpeed = 127;
// Vitesse courante du moteur gauche
int speedLeft = 0;
// Vitesse courante du moteur droit
int speedRight = 0;
// Sens du moteur gauche
int motorLeftRun = RELEASE;
// Sens du moteur droit
int motorRightRun = RELEASE;

// Contrôle du servomoteur 1
Servo myservo;
// Contrôle du servomoteur 2
Servo myservo2;

// Moteur à courant continue de la roue gauche
AF_DCMotor motorLeft(1/*, MOTOR12_64KHZ*/);
// Moteur à courant continue de la roue droite
AF_DCMotor motorRight(2/*, MOTOR12_64KHZ*/);
 

// Méthode d'initalisation
void setup() 
{
  // On place les borches 30 et 31 en mode sortie (pour le relais)  
  pinMode(30, OUTPUT); 
  pinMode(31, OUTPUT); 

  // On attache les servo moteurs aux broches 9 et 10 (comme indiqué ici : https://learn.adafruit.com/adafruit-motor-shield/faq)
  myservo.attach(9);  // attaches the servo on pin 9 to the servo object
  myservo2.attach(10);  // attaches the servo on pin 9 to the servo object

  // On initialise le port série par dafaut (celui disponible dans l'editeur via USB) pour du debug
  Serial.begin(9600);
  // On écrit un message de test sur la liaison série
  Serial.println("Adafruit VS1053 Simple Test");
  
  // On initialise le port série numéri 1, disponible sur les pins TX1 (18) et RX1 (19) de la carte Arduino Mega
  // C'est ce port série qui est utilisé pour communiquer avec le module bluetooth qui sera connecté au téléphone
  Serial1.begin(9600);
  // On écrit un message de test sur la liaison série
  Serial1.println("Adafruit VS1053 Simple Test");

  // Initialisation du player de mp3
  if (! musicPlayer.begin()) 
  {  // Ca c'est ma passé
     Serial.println(F("Couldn't find VS1053, do you have the right pins defined?"));
     // On ne va pas plus loin
     while (1);
  }
  // Message de debug pour indiquer que le lecteur mp3 est bien initialisé
  Serial.println(F("VS1053 found"));

  // On initialise la carte SD
  SD.begin(CARDCS);
  
  // On définit le volume de sortie droite et gauche (on n'utilise en fait que le droit car on n'a qu'un haut-parleur)
  // Attention : plus le nombre est petit plus le volume est fort !
  musicPlayer.setVolume(10,10);

  // On dit au lecteur d'utiliser le mode "interruption" : cela permet de lire des sons en tache de fond, sans bloquer l'application
  // C'est possible car un utiliser une borche d'interruption de la carte Arduino Mega (DREQ = 21) qui permet au shield mp3 de signaler
  // à la carte arduino qu'elle doit interrompre le code en cours pour exécuter le code de l'interruptio à certain moments 
  // (pour aller chercher des données à lire sur la carte SD par exemple)
  musicPlayer.useInterrupt(VS1053_FILEPLAYER_PIN_INT);  // DREQ int
  
  // Exemple de lecture d'un fichier audio de façon blocante (on ne passe pas à l'instruction suivante tant que le fichier n'est pas lu entièrement)
  /*Serial.println(F("Playing track 001"));
  musicPlayer.playFullFile("freins.mp3");*/
  // Exemple de lecture en tache de fond
  /*Serial.println(F("Playing track 002"));
  musicPlayer.startPlayingFile("klaxon.mp3");*/
}

void loop() 
{
  // Traitement des données du port série USB
  processUsbSerial();
  processBluetoothSerial();

  delay(10);
}

/**
 * Traitement des données du port série blueetoth :
 * - Si on reçoit "x<valeur>\n" => position du joystick gauche / droite
 * - Si on reçoit "y<valeur>\n" => position du joystick avant / arrière
 * - Si on reçoit "k<0/255>\n" => klaxon
 * - Si on reçoit "p<0/255>\n" => phare
 */
void processBluetoothSerial()
{
  if (Serial1.available()>=1) 
  { // Une commande est disponible
    char curChar = Serial1.read();
    Serial.println(curChar);
    processCommand(curChar);
 }
}

/**
 * Traitement des commandes
 * @param command :
Forward---------------------F
Back-------------------------B
Left---------------------------L
Right-------------------------R
Forward Left--------------G
Forward Right------------I
Back Left------------------H
Back Right----------------J
Stop-------------------------S
Front Lights On---------W
Front Lights Off---------w (lower case)
Back Lights On---------U
Back Lights Off---------u (lower case)
Speed 0-------------------0
Speed 10-----------------1
Speed 20-----------------2
Speed 30-----------------3
Speed 40-----------------4
Speed 50-----------------5
Speed 60-----------------6
Speed 70-----------------7
Speed 80-----------------8
Speed 90-----------------9
Speed 100---------------q
Everything OFF--------D
 */
void processCommand(char command)
{
  switch(command)
  {
    case 'F':
      // En avant
      currentX = MID_VALUE+currentSpeed;
      currentY = MID_VALUE;
      // Mise à jour des moteurs
      updateDcMotors();
      break;
    case 'B':
      // En arrière
      currentX = MID_VALUE-currentSpeed;
      currentY = MID_VALUE;
      // Mise à jour des moteurs
      updateDcMotors();
      break;
    case 'L':
      // A gauche
      currentX = MID_VALUE;
      currentY = MID_VALUE-currentSpeed;
      // Mise à jour des moteurs
      updateDcMotors();
      break;
    case 'R':
      // A droite
      currentX = MID_VALUE;
      currentY = MID_VALUE+currentSpeed;
      // Mise à jour des moteurs
      updateDcMotors();
      break;
    case 'G':
      // Avant gauche
      currentX = MID_VALUE+currentSpeed;
      currentY = MID_VALUE-currentSpeed;
      // Mise à jour des moteurs
      updateDcMotors();
      break;
    case 'I':
      // Avant droite
      currentX = MID_VALUE+currentSpeed;
      currentY = MID_VALUE+currentSpeed;
      // Mise à jour des moteurs
      updateDcMotors();
      break;
    case 'H':
      // Arrière gauche
      currentX = MID_VALUE-currentSpeed;
      currentY = MID_VALUE-currentSpeed;
      // Mise à jour des moteurs
      updateDcMotors();
      break;
    case 'J':
      // Arrière droite
      currentX = MID_VALUE-currentSpeed;
      currentY = MID_VALUE+currentSpeed;
      // Mise à jour des moteurs
      updateDcMotors();
      break;
    case 'S':
    case 'D':
      // Mise à jour de la valeur x
      currentX = MID_VALUE;
      currentY = MID_VALUE;
      // Mise à jour des moteurs
      updateDcMotors();
      break;
    case 'W':
      relay2 = true;
      digitalWrite(31, relay2);
      break;
    case 'w':
      relay2 = false;
      digitalWrite(31, relay2);
      break;
    case 'U':
      relay1 = true;
      digitalWrite(30, relay1);
      break;
    case 'u':
      relay1 = false;
      digitalWrite(30, relay1);
      break;
    case 'V':
    case 'v':
      klaxon();
      break;
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
      currentSpeed = map(command-'0', 0, 9, 0, 127);
      break;
  }
}

void klaxon()
{
  musicPlayer.stopPlaying();
  musicPlayer.startPlayingFile("klaxon3.mp3");
}

void updateDcMotors()
{
  if(currentX < MID_VALUE)
  { // Le joystick avant / arrière est vers l'arrière => on recule
    int motorSpeed = map(currentX, MID_VALUE-1, 0, MIN_SPEED, 255);
    moveOneWay(motorSpeed,false);
  }
  else if(currentX > MID_VALUE)
  { // Le joystick avant / arrière est vers l'avant => on avance
    int motorSpeed = map(currentX, MID_VALUE+1, 255, MIN_SPEED, 255);
    moveOneWay(motorSpeed,true);
  }
  else
  { // Le joystick avant / arrière est  à 0 => on regarde s'il faut tourner sur place
    if(currentY < MID_VALUE)
    { // On tourne sur place vers la gauche
      speedLeft = map(currentY, MID_VALUE-1, 0, MIN_SPEED, 255);
      speedRight = speedLeft;
      motorLeftRun = BACKWARD;
      motorRightRun = FORWARD;
    }
    else if(currentY > MID_VALUE)
    { // On tourne sur place vers la droite
      speedLeft = map(currentY, MID_VALUE+1, 255, MIN_SPEED, 255);
      speedRight = speedLeft;
      motorLeftRun = FORWARD;
      motorRightRun = BACKWARD;
    }
    else
    { // Tout est à 0 => on éteint les moteurs
      motorLeftRun = RELEASE;
      motorRightRun = RELEASE;
    }
  }
  motorLeft.setSpeed(speedLeft);
  motorRight.setSpeed(speedRight);
  motorLeft.run(motorLeftRun);
  motorRight.run(motorRightRun);
  /*Serial.print("Motor left : speed = ");
  Serial.print(speedLeft,DEC);
  Serial.print(" run = ");
  Serial.println(motorLeftRun,DEC);
  Serial.print("Motor right : speed = ");
  Serial.print(speedRight,DEC);
  Serial.print(" run = ");
  Serial.println(motorRightRun,DEC);*/
}

/**
 * Déplacer la voiture dans une direction
 * @param motorSpeed vitesse des moteurs
 * @param goForward si vrai, on va en avant, sinon on recule
 */
void moveOneWay(int motorSpeed, bool goForward)
{
  if(goForward)
  {
      motorLeftRun = FORWARD;
      motorRightRun = FORWARD;
  }
  else
  {
      motorLeftRun = BACKWARD;
      motorRightRun = BACKWARD;
  }
  int diff;
  speedLeft = motorSpeed;
  speedRight = motorSpeed;
  if(currentY < MID_VALUE)
  { // On tourne à gauche
    diff = map(currentY, 0, MID_VALUE-1, MIN_SPEED, 255);
    speedLeft -= diff;
    speedRight += diff;
  }
  else if(currentY > MID_VALUE)
  { // On tourne à droite
    diff = map(currentY, MID_VALUE+1, 255, MIN_SPEED, 255);
    speedLeft += diff;
    speedRight -= diff;
  }
  if(speedLeft < MIN_SPEED) 
  {
    speedLeft = MIN_SPEED;
    /*if(currentY <= LEFT_TURN_THRESHOLD)
    {
      motorLeftRun = RELEASE;
    }*/
  }
  if(speedRight < MIN_SPEED) 
  {
    speedRight = MIN_SPEED;
    /*if(currentY >= RIGHT_TURN_THRESHOLD)
    {
      motorRightRun = RELEASE;
    }*/
  }
  if(speedRight > 255) 
  {
    speedRight = 255;
  }
  if(speedLeft > 255) 
  {
    speedLeft = 255;
  }
}

void processUsbSerial()
{
  if (Serial.available()) 
  {
    char c = Serial.read();
    Serial.println("Read char:");
    
    // if we get an 's' on the serial console, stop!
    if (c == 's') {
      musicPlayer.stopPlaying();
    }
    
    // if we get an 'p' on the serial console, pause/unpause!
    if (c == 'p') {
      if (! musicPlayer.paused()) {
        Serial.println("Paused");
        musicPlayer.pausePlaying(true);
      } else { 
        Serial.println("Resumed");
        musicPlayer.pausePlaying(false);
      }
    }

    // if we get an 's' on the serial console, stop!
    if (c == '1') {
      Serial.println(F("Playing klaxon1"));
      musicPlayer.stopPlaying();
      musicPlayer.startPlayingFile("klaxon.mp3");
    }
    
    // if we get an 's' on the serial console, stop!
    if (c == '2') {
      Serial.println(F("Playing klaxon2"));
      musicPlayer.stopPlaying();
      musicPlayer.startPlayingFile("klaxon2.mp3");
    }
    
    // if we get an 's' on the serial console, stop!
    if (c == '3') {
      Serial.println(F("Playing klaxon3"));
      musicPlayer.stopPlaying();
      musicPlayer.startPlayingFile("klaxon3.mp3");
    }
    
    // if we get an 's' on the serial console, stop!
    if (c == '0') {
      Serial.println(F("Playing freins"));
      musicPlayer.stopPlaying();
      musicPlayer.startPlayingFile("freins.mp3");
    }
    
    if (c == '7') {
      relay1 = !relay1;
      digitalWrite(30, relay1);
    }
    if (c == '8') {
      relay2 = !relay2;
      digitalWrite(31, relay2);
    }
    
    if (c == '+') {
  motorLeft.setSpeed(200);     // set the speed to 200/255
  motorRight.setSpeed(200);     // set the speed to 200/255
    motorLeft.run(FORWARD);      // turn it on going forward
    motorRight.run(FORWARD);      // turn it on going forward
    }

    if (c == '-') {
  motorLeft.setSpeed(200);     // set the speed to 200/255
  motorRight.setSpeed(200);     // set the speed to 200/255
    motorLeft.run(RELEASE);      // turn it on going forward
    motorRight.run(RELEASE);      // turn it on going forward
    }
    
    if (c == 't') {
         Serial1.println("Test !");
    }
  }
}
