# Feuille de route

## Jalon 1 — Noyau logiciel

- [x] Modèle d'état du véhicule et qualité des signaux
- [x] Politique de sécurité indépendante du matériel
- [x] Machine d'état événementielle
- [x] Détecteur borné de trois impulsions de verrouillage
- [x] Session distante de 15 minutes et reprise conducteur temporisée
- [x] Modèle de configuration utilisateur validé et borné
- [x] Fichier de configuration strict pour le simulateur
- [x] Journal binaire redondant, versionné et protégé par CRC
- [x] Récupération après corruption ou écriture interrompue
- [x] Configurateur PC interactif et écriture de fichier vérifiée
- [x] Protocole de configuration indépendant du transport avec CRC
- [x] Autorisation obligatoire et écriture limitée à l'état `Idle`
- [x] Récepteur de flux borné avec délai inter-octets et resynchronisation
- [x] Endpoint de configuration raccordable à un transport abstrait
- [x] Actions bornées sans allocation dynamique
- [x] Runtime et ports abstraits
- [x] Superviseur logiciel d'actionneurs avec heartbeat et défauts mémorisés
- [x] Tests hôte et intégration continue
- [x] Firmware de référence inerte

## Jalon 1.1 — Simulation et rejeu

- [x] Modèle de trame CAN classique/étendue validé
- [x] Rejeu temporel sans allocation dynamique
- [x] Décodage atomique en lots bornés
- [x] Calcul de fraîcheur individuel des signaux
- [x] Gateway de rejeu compatible avec le runtime
- [x] Protocole synthétique explicitement séparé de BMW
- [x] Simulateur nominal avec injection d'un interverrouillage
- [x] Exécutable interactif avec scénarios capot obligatoire et facultatif
- [x] Interface graphique Windows locale du simulateur
- [x] Bac à sable graphique persistant avec état véhicule et défauts injectables
- [x] Scénario de bout en bout runtime et superviseur d'actionneurs
- [x] Format de trace canonique strict et importeur hôte `python-can`
- [x] Simulateur capable de charger une trace externe

## Jalon 1.2 — Profils véhicule

- [x] Signaux sémantiques indépendants des protocoles
- [x] Registre fixe de profils extensibles
- [x] Profil de découverte E90 2009 N47D20C boîte automatique
- [x] Garde de qualification fermé par défaut
- [ ] Ajouter les variantes E9x à partir de données validées
- [x] Sélectionner explicitement un profil dans la configuration

## Jalon 1.3 — Découverte hors ligne

- [x] Lecteur Python strict du format canonique
- [x] Comparaison statistique de deux traces
- [x] Classement des octets et bits candidats
- [x] Scénario synthétique capot fermé / ouvert
- [x] Protocole de collecte répétable en lecture seule
- [x] Outil de capture bornée pour pilotes au mode silencieux documenté
- [x] Fiche privée reproductible pour l'inventaire diagnostic K+DCAN
- [ ] Qualifier une interface de capture brute compatible E9x
- [ ] Collecter les premières paires privées sur le véhicule de référence

## Jalon 1.4 — Plateforme fonctionnelle modulaire

- [x] Catalogue stable des 43 fonctionnalités proposées
- [x] Activation indépendante, désactivée par défaut
- [x] Classification lecture seule, confort futur et banc uniquement
- [x] Résolution séparée des préférences, capacités et qualifications
- [x] Persistance V2 du masque avec migration automatique de la V1
- [x] Compatibilité de modèle pour compagnons iOS ou Android
- [x] Ajouter le premier moteur V1 lecture seule : moteur froid, FAP et huile de boîte
- [ ] Ajouter les autres comportements V1 de télémétrie en simulation
- [ ] Concevoir le protocole applicatif commun aux compagnons mobiles
- [ ] Développer le premier compagnon iOS
- [ ] Développer ensuite le compagnon Android
- [ ] Ajouter une interface Web embarquée après choix et revue du modèle d'accès

## Jalon 2 — Spécification matérielle

- [x] Choisir l'ESP32-S3-DevKitC-1-N8 pour le prototype de banc
- [x] Retenir l'USB filaire comme transport local de configuration V1
- [ ] Concevoir la carte automobile définitive après qualification des bus
- [ ] Optionnel : concevoir une entrée indépendante pour les installations qui exigent le capot
- [ ] Établir la matrice complète des variantes E9x visées
- [ ] Identifier les signaux nécessaires et leurs sources légitimes
- [ ] Définir les seuils de fraîcheur et les critères de plausibilité
- [ ] Concevoir les interverrouillages matériels et le watchdog
- [ ] Réaliser l'analyse de risques et le plan de test

## Jalon 3 — Communications et commande

- [ ] Implémenter le décodeur véhicule réel en lecture seule
- [x] Tester perte, retard et corruption des données
- [x] Implémenter une garde abstraite de provenance, fraîcheur et anti-rejeu
- [x] Ajouter le pipeline CAN de verrouillage paramétrable et fermé par défaut
- [ ] Qualifier la provenance BMW des verrouillages et la résistance au rejeu
- [ ] Implémenter l'adaptateur réel de confirmation de reprise conducteur
- [x] Ajouter une journalisation bornée et non sensible
- [x] Implémenter `SettingsByteStorage` dans la mémoire flash du prototype
- [x] Choisir l'USB filaire entre le configurateur et le boîtier V1
- [x] Implémenter le transport USB sur ESP32-S3
- [x] Raccorder le configurateur Windows au port série USB avec relecture
- [x] Identifier la cible et ses capacités avant toute écriture USB
- [x] Automatiser le flashage et le premier test USB du prototype
- [ ] Implémenter l'authentification et l'anti-rejeu propres au transport choisi
- [ ] Valider la consommation au repos

## Jalon 4 — Actionneurs sur banc

- [ ] Implémenter `ActuatorPort` avec sorties inactives par défaut
- [ ] Ajouter le retour d'état et les diagnostics électriques
- [ ] Tester sur charges factices
- [ ] Réaliser les scénarios HIL et les campagnes d'injection de défauts

## Jalon 5 — Intégration contrôlée

- [ ] Revue indépendante du logiciel, du schéma et des risques
- [ ] Qualification des paramètres par variante véhicule
- [ ] Essais progressifs selon `docs/hardware-integration.md`
- [ ] Documentation d'installation et procédure de retour à l'origine
