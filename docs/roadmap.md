# Feuille de route post-audit

Cette feuille de route remplace les anciens jalons qui mélangeaient code livré,
idées et matériel futur. Les phases sont séquentielles : aucune phase ne doit
commencer sans validation explicite de la précédente.

## Phase 1 — Simplification post-audit

Objectif : réduire la dette et établir une description fidèle de l'existant,
sans ajouter de comportement.

- [x] Limiter le catalogue aux trois fonctions réellement implémentées
- [x] Déplacer les 40 idées restantes dans `docs/backlog/features.md`
- [x] Supprimer les migrations de configuration pré-V1 inutiles
- [x] Centraliser le calcul CRC32
- [x] Utiliser un manifeste unique pour les sources CMake et PowerShell
- [x] Commencer la séparation de `tests/test_main.cpp` sans réécrire les tests
- [x] Ajouter les builds PlatformIO natif et ESP32-S3 à la CI
- [x] Aligner la documentation sur l'état réellement livré
- [x] Retirer MCP2515 de la cible matérielle officielle
- [x] Valider tous les tests et builds locaux, puis la CI GitHub

## Phase 2 — Socle ESP32-S3 sûr

- [x] Adopter ESP-IDF 5.5.0 comme environnement embarqué de référence
- [x] Versionner `sdkconfig.defaults` et la table de partitions
- [x] Remplacer l'adaptateur Arduino/EEPROM par NVS
- [x] Définir la HAL minimale GPIO, temps, stockage et sûreté du futur TWAI
- [x] Activer watchdogs et buffers USB bornés sans tâche applicative superflue
- [x] Documenter silence matériel, inhibition TX et validation sur banc
- [x] Prouver au build l'absence d'installation et d'émission TWAI

## Phase 3 — TWAI en écoute seule

- [x] Définir le port de réception CAN indépendant d'Espressif et de BMW
- [x] Implémenter l'adaptateur ESP32-S3/TWAI en mode écoute seule
- [x] Garantir par construction et en CI l'absence d'émission
- [x] Ajouter file fixe, compteurs, erreurs et horodatage bornés
- [x] Définir le format Capture V2 et son manifeste de session

## Phase 3B — Matériel CAN de banc

- [x] Comparer les transceivers automobiles à partir des datasheets constructeur
- [x] Retenir le TCAN1057AV-Q1 et une inhibition TX physique indépendante
- [x] Définir le schéma de principe, les pulls sûrs et les GPIO BENCH_ONLY
- [x] Définir la procédure électrique et la checklist PASS/FAIL
- [ ] Assembler et inspecter le banc physique
- [ ] Exécuter et archiver tous les essais électriques
- [ ] Obtenir un PASS complet avant toute demande de connexion véhicule

## Phase 3C — Préparation des données véhicule

- [x] Définir un profil versionné pour l'identification réelle du véhicule
- [x] Définir une session ordonnée d'observation du démarrage OEM
- [x] Indexer la provenance ISTA, TestO, INPA, Tool32, capture CAN ou note manuelle
- [x] Ajouter un import tabulaire générique sans dépendance propriétaire
- [x] Créer la checklist versionnée des prérequis remote-start
- [x] Valider schémas, champs obligatoires, confiance, provenance et cohérence
- [x] Intégrer les relevés réels après réception et revue des données utilisateur

La Phase 3C reste strictement documentaire et hors véhicule. Elle ne décode
aucun signal BMW et n'ajoute ni émission CAN, ni commande CAS/DDE. Son contrat
est détaillé dans
[phase3c-vehicle-data-intake.md](phase3c-vehicle-data-intake.md).

## Phase 3D — Qualification des preuves réelles

- [x] Qualifier les sources signal par signal et par usage
- [x] Archiver une timeline synchronisée RPM/MSA sans inférer la fonction des bits
- [x] Conserver les bitfields en décimal, hexadécimal, binaire et bits modifiés
- [x] Interdire la promotion automatique d'une corrélation vers une sémantique fonctionnelle
- [x] Confirmer l'état KL50 en lecture sans inférer son mécanisme d'activation
- [x] Corroborer la transition KL50 par un trace IFH Level 1
- [x] Borner la durée KL50 avec les timestamps d'un trace IFH Level 3
- [x] Archiver une corrélation diagnostique CAS/DDE sur une horloge commune
- [x] Archiver deux démarrages CAS/DDE et comparer leurs bornes sans créer d'algorithme
- [x] Archiver un cycle OEM complet incluant la décroissance RPM après arrêt
- [x] Séparer les actions utilisateur déclarées des signaux réellement mesurés
- [ ] Résoudre l'ordre physique KL50/RPM avec une acquisition non ambiguë
- [ ] Valider l'algorithme de détection moteur tournant sur plusieurs captures structurées
- [ ] Identifier Terminal 50, la demande START et l'autorisation OEM par preuves séparées

- [x] Intégrer les identifications réelles CAS, DDE et EGS avec provenance
- [x] Qualifier KL15, P/R/N/D, frein et régime moteur par source
- [x] Marquer le mapping ISTA Transmission position comme `UNTRUSTED`
- [x] Interdire structurellement les données `UNTRUSTED` ou `BLOCKED` comme préconditions candidates
- [x] Ajouter le modèle observationnel `EngineRunStateObservation`
- [x] Préparer l'import générique d'un CSV de régime TestO
- [x] Archiver l'ordre réel des RPM sans inventer de timestamps
- [ ] Importer le CSV TestO brut et produire sa timeline réellement horodatée
- [ ] Valider les seuils candidats sur plusieurs démarrages OEM indépendants

Les résultats et limites de cette phase sont détaillés dans
[phase3d-real-evidence-qualification.md](phase3d-real-evidence-qualification.md).

## Phase 3E — Préparation K-CAN passive

- [x] Distinguer débit logique, contrôleur TWAI et couche physique K-CAN
- [x] Comparer les transceivers LS/FT à partir des documents constructeur
- [x] Retenir `TJA1055T/3/2Z` comme candidat principal de prototype
- [x] Définir deux barrières TX matérielles en série, dont un lien DNP
  indépendant du firmware
- [x] Conserver `0x23A` et `0x2B4` comme hypothèses externes non validées
- [x] Définir la matrice d'essais télécommande et les contrôles négatifs
- [ ] Assembler le récepteur K-CAN RX-only
- [ ] Qualifier alimentation, reset, brownout, absence d'ACK et absence de dominant
- [ ] Mesurer l'influence de la terminaison et du mode normal sur la veille K-CAN
- [ ] Obtenir un PASS électrique complet avant toute connexion au véhicule
- [ ] Ajouter le profil firmware `BENCH_ONLY` K-CAN après qualification du matériel
- [ ] Réaliser les captures réelles, sans filtre d'identifiants

Cette phase est détaillée dans
[phase3e-kcan-passive-acquisition.md](phase3e-kcan-passive-acquisition.md).
Elle ne commence ni la Phase 4, ni une fonction de commande véhicule.

## Phase 3F — Gel du design K-CAN RX-only

- [x] Auditer les 14 broches du `TJA1055T/3/2Z`
- [x] Corriger le maintien récessif de TXD pour qu'il dépende du VCC 5 V du
  transceiver et non du domaine 3,3 V
- [x] Figer une coupure TX physiquement absente (`R_LINK_TX` DNP)
- [x] Rendre l'activation STB/EN manuelle et indépendante du firmware
- [x] Figer le câblage, la nomenclature et les points de test du prototype
- [x] Quantifier la charge des résistances RTH/RTL de 5,62 kΩ
- [x] Comparer les moyens de générer un réseau LS/FT de banc à 100 kbit/s
- [x] Ajouter des contrôles documentaires sur la BOM et le netlist
- [x] Auditer l'approvisionnement ligne par ligne, corriger les références
  retirées/incompatibles et figer une liste d'achat versionnée
- [x] Intégrer les corrections procurement post-freeze de la PR #34 sans
  modifier la BOM ni le design électrique
- [ ] Assembler le prototype
- [ ] Qualifier électriquement alimentation, reset, brownout, ACK et absence de
  dominant avec oscilloscope et générateur LS/FT
- [ ] Obtenir un PASS signé avant toute étude de raccordement véhicule

Le gel est détaillé dans
[phase3f-kcan-rxonly-design-freeze.md](phase3f-kcan-rxonly-design-freeze.md) et
dans `hardware/kcan-rxonly/`. Il ne constitue pas une autorisation de connexion
à la BMW et ne commence pas la Phase 4.

## Phase 3G — Prototype PCB manufacturable K-CAN RX-only

- [x] Ré-auditer les composants, brochages, modes du TJA1055, GPIO et sources
  constructeurs avant tout dessin
- [x] Vérifier la faisabilité PCB/PCBA et les options de sourcing fabricant
- [x] Identifier le blocker du rail `5V_TJA` historique et arrêter avant layout
- [x] Faire approuver une correction Phase 3G par `TLS715B0EJV50` dédié
- [x] Refaire budget de tension, thermique, séquencement et backfeed
- [x] Créer le schéma et le PCB KiCad natifs deux couches
- [x] Exécuter ERC, DRC et revue pin-à-pin : zéro erreur/violation
- [x] Préserver une coupure TX sans composant, cuivre, via ou zone commune
- [x] Qualifier le sourcing exact sans substitution électrique
- [x] Générer puis reparcourir le package de fabrication et les Gerbers
- [ ] Obtenir une validation humaine avant toute commande

L'audit historique et sa résolution sont détaillés dans
[phase3g-pcb-feasibility-audit.md](phase3g-pcb-feasibility-audit.md) et
[phase3g-pcb-manufacturing.md](phase3g-pcb-manufacturing.md). Les exports sont
des livrables de revue seulement : aucune commande n'a été passée et cette
phase ne commence pas la Phase 4.

## Phase 3G.1 — Optimisation DFM et coût du PCB

- [x] Identifier les neuf vias 0,50/0,20 mm dans le pad U3 comme cause du
  process via-in-pad spécial observé au premier DFM JLCPCB
- [x] Revalider le pad exposé et le stencil à partir du boîtier Infineon
  `PG-DSO-8-52`
- [x] Démontrer l'enveloppe thermique avec la donnée conservatrice 153 K/W,
  sans créditer le cuivre étendu ni les vias périphériques
- [x] Supprimer tous les trous du pad U3 et employer quatre vias GND
  périphériques tentés 0,60/0,30 mm
- [x] Conserver l'architecture électrique, la barrière TX et le firmware
  strictement inchangés
- [x] Régénérer et valider le package de fabrication complet
- [ ] Recharger le ZIP Phase 3G.1 dans le configurateur et confirmer le DFM et
  le coût réels avant toute commande

La décision et ses limites sont détaillées dans
[phase3g1-dfm-cost-optimization.md](phase3g1-dfm-cost-optimization.md). Elle
n'autorise ni commande fabricant, ni connexion BMW, ni Phase 4.

## Phase 3H — matériel K-CAN entièrement bidirectionnel

- [x] Conserver `TJA1055T/3/2Z` et le power tree Phase 3G.1
- [x] Remplacer le buffer mono-alimentation par un traducteur automobile
  double alimentation `SN74LXC1T45QDCKRQ1`
- [x] Router un chemin TX continu de GPIO5/TWAI_TX jusqu'à TXD du TJA1055
- [x] Conserver le chemin RX de RXD vers GPIO4/TWAI_RX
- [x] Supprimer jumper, pont, DNP, keepout et coupure cuivre TX
- [x] Conserver l'optimisation U3 sans via-in-pad et ses vias 0,60/0,30 mm
- [x] Ajouter au firmware le choix explicite `LISTEN_ONLY`/`NORMAL`, sans API TX
- [x] Régénérer Gerbers, drills, IPC-D-356, BOM/CPL, JLCPCB, NextPCB, STEP,
  PDF, rendus, ZIP et manifeste SHA-256
- [x] Valider ERC, DRC, chemins TX/RX et absence de piste non routée
- [ ] Assembler et qualifier électriquement la carte sur banc
- [ ] Obtenir une autorisation séparée avant toute connexion ou émission BMW

La Phase 3H remplace Phase 3G/3G.1 uniquement comme révision à fabriquer ; les
anciennes révisions et leurs tags restent historiques. Elle n'ajoute aucun ID
BMW, aucune commande véhicule, aucun contournement CAS/EWS et ne commence pas
la Phase 4. Voir
[phase3h-bidirectional-kcan.md](phase3h-bidirectional-kcan.md).

## Phase 4 — Capture et analyse hors ligne

- [ ] Implémenter le transport et l'écriture PC du format Capture V2
- [ ] Valider et stabiliser le format avec des captures de banc
- [ ] Renforcer l'analyse différentielle et les rapports reproductibles
- [ ] Ajouter les jeux de validation sans données personnelles

## Phase 5 — Base de données des trames

- [ ] Définir un schéma versionné pour identifiants, signaux et preuves
- [ ] Enregistrer véhicule, variante, provenance et date de validation
- [ ] Gérer niveaux de confiance et contradictions
- [ ] Valider le schéma avec des captures de plusieurs véhicules

## Phase 6 — Bibliothèque BMW E8x/E9x

- [ ] Créer une bibliothèque indépendante du contrôleur distant
- [ ] Décoder uniquement les trames suffisamment validées
- [ ] Documenter unités, cadence, compteurs et contrôles d'intégrité
- [ ] Tester la compatibilité inter-véhicules et les cas inconnus

## Phase 7 — API véhicule en lecture seule

- [ ] Exposer des signaux sémantiques qualifiés au domaine
- [ ] Relier fraîcheur, plausibilité et profils véhicule
- [ ] Remplacer progressivement les sources synthétiques dans les essais de banc
- [ ] Conserver toutes les commandes véhicule désactivées

## Phase 8 — Commande sur banc et sûreté matérielle

- [ ] Réaliser l'analyse de risques avant toute sortie physique
- [ ] Concevoir alimentation, protections, watchdog et interverrouillages
- [ ] Implémenter les actionneurs uniquement sur charges factices
- [ ] Mener les campagnes HIL et d'injection de défauts

## Phase 9 — Interfaces utilisateur

- [ ] Stabiliser une API authentifiée et anti-rejeu
- [ ] Concevoir l'application iOS
- [ ] Ajouter un client Android sur la même API
- [ ] Maintenir chaque option désactivable dans les limites de sûreté

## Phase 10 — Qualification véhicule contrôlée

- [ ] Revue indépendante du logiciel, du schéma et des risques
- [ ] Qualification par variante E8x/E9x
- [ ] Essais progressifs avec faisceau réversible
- [ ] Documentation d'installation et procédure de retour à l'origine

Les idées fonctionnelles non planifiées restent dans
[backlog/features.md](backlog/features.md) et n'entrent dans aucune phase sans
revue explicite.
