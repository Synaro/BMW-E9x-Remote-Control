# Feuille de route

## Jalon 1 — Noyau logiciel

- [x] Modèle d'état du véhicule et qualité des signaux
- [x] Politique de sécurité indépendante du matériel
- [x] Machine d'état événementielle
- [x] Actions bornées sans allocation dynamique
- [x] Runtime et ports abstraits
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
- [ ] Importer le format de capture retenu pour le matériel réel

## Jalon 2 — Spécification matérielle

- [ ] Choisir la carte après étude alimentation, température et E/S
- [ ] Établir la matrice exacte des variantes E9x visées
- [ ] Identifier les signaux nécessaires et leurs sources légitimes
- [ ] Définir les seuils de fraîcheur et les critères de plausibilité
- [ ] Concevoir les interverrouillages matériels et le watchdog
- [ ] Réaliser l'analyse de risques et le plan de test

## Jalon 3 — Communications et commande

- [ ] Implémenter le décodeur véhicule réel en lecture seule
- [ ] Tester perte, retard et corruption des données
- [ ] Définir une commande distante authentifiée et résistante au rejeu
- [ ] Ajouter une journalisation bornée et non sensible
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
