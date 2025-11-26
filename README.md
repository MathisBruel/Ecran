# Alarme Connectée — Système de Sécurité Intelligent

Projet IoT : système d'alarme connecté combinant Arduino, Raspberry Pi et ESP32 pour la détection et l'alerte en temps réel.

---

## 🚀 Présentation

Ce projet a été développé dans le cadre d'un projet d'étude afin de concevoir un système de sécurité modulaire et connecté. L'objectif est de surveiller un espace en temps réel, de prendre des photos à la détection, et d'alerter l'utilisateur via Telegram. Le système sert aussi d'exercice pratique sur l'IoT, l'architecture réseau et l'intégration multi-plateforme.

Points clés :
- Unité centrale : Arduino Uno (gestion capteurs, alarmes, interface)
- Détection : ESP32 (capteur ultrason / mouvement)
- Photo & sauvegarde : Raspberry Pi (prise de photo lors d'une alerte)
- Notifications : API Telegram
- Backend léger : API/serveur Flask pour la coordination et l'interface web


---

## 🛠️ Technologies & matériels

Hardware
- Arduino Uno — gestion capteurs et hub central
- ESP32 — détection de mouvement (ultrason)
- Raspberry Pi 3 — prise de photo et connexion serveur
- Capteurs : infrarouge (PIR), capteur ultrason, modules divers (buzzer, LEDs, écran)
- RFID / clavier pour activation/désactivation (option)

Backend
- Python 3, Flask
- Utilisation de Telegram Bot API pour notifications

Frontend
- HTML5 / CSS3 / JavaScript (interface web responsive, contrôle d'état)

Réseau & IoT
- WiFi, WebSocket (communication temps réel entre modules et serveur)

---

## 🧩 Fonctionnalités

- Surveillance continue et détection de mouvements
- Photographie automatique via Raspberry Pi lors d'une alerte
- Notifications instantanées sur Telegram (texte + photo)
- Activation/désactivation via code ou carte RFID
- Indication d'état via écran et LEDs, déclenchement buzzer
- Historique des événements stocké côté serveur (logs)
- Boîtier imprimé en 3D pour protection physique de l'unité centrale

---

## 🔧 Défis rencontrés & solutions

1. Communication inter-modules
- Défi : latence et synchronisation entre Arduino, ESP32 et Pi.
- Solution : centralisation via un serveur Flask, communications HTTP/WebSocket légères.

2. Réglages caméra
- Défi : paramétrer angle, focus et luminosité pour de bonnes photos.
- Solution : tests sur Raspberry Pi Camera, ajustement des paramètres et post-traitement minimal.

3. Boîtier imprimé 3D
- Défi : conception et ajustements dimensionnels.
- Solution : modélisation (Fusion 360) et itérations d'impression.

---

## 👤 Auteur

Mathis Bruel & Hugo Meuriel — Étudiant en informatique (SupDeVinci)  
Développeur IoT / DevOps junior

Crédits : MEURIEL Hugo
