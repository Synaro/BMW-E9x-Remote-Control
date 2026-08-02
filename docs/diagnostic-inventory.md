# Inventaire diagnostic K+DCAN en lecture seule

## Objectif

Cette phase décrit ce que le véhicule expose déjà par diagnostic avant de
chercher des trames CAN brutes. Elle sert à confirmer les variantes de
calculateurs et les valeurs d'état disponibles sur le véhicule de référence.

Le câble K+DCAN et les logiciels BMW restent des outils de diagnostic : ils
émettent des requêtes. « Lecture seule » signifie ici qu'aucune fonction de
codage, programmation, effacement, adaptation ou activation d'actionneur n'est
lancée.

## Créer la session privée

Depuis la racine du dépôt :

```powershell
.\scripts\new-diagnostic-session.ps1 -SessionName e90-reference-01
```

La commande crée :

```text
captures/private/e90-reference-01/
├── diagnostic-inventory.md
└── screenshots/
```

Le répertoire `captures/private/` est exclu de Git. Ne jamais déplacer son
contenu dans `docs/`, une issue ou une pull request sans anonymisation et revue.

## Procédure d'observation

1. immobiliser le véhicule, sélectionner `P` et appliquer le frein de
   stationnement ;
2. stabiliser l'alimentation conformément à la procédure de l'outil utilisé ;
3. lancer uniquement l'identification du véhicule et la lecture de
   l'arborescence des calculateurs ;
4. recopier les noms de variantes et versions utiles dans la fiche privée, sans
   enregistrer le VIN complet ;
5. rechercher les valeurs d'état, sans lancer de plan de test ayant un effet sur
   le véhicule ;
6. pour chaque valeur, observer au moins deux états simples et réversibles avec
   moteur arrêté lorsque c'est possible ;
7. conserver les captures d'écran anonymisées dans le sous-dossier privé.

Ne pas lancer :

- codage ou programmation ;
- effacement des défauts ;
- apprentissage ou adaptation ;
- activation de relais, pompe, démarreur ou autre actionneur ;
- job Tool32 dont le résultat ou les effets ne sont pas établis.

## Calculateurs prioritaires

Les noms exacts dépendent de la variante et doivent être recopiés tels qu'ils
apparaissent dans l'outil. Les rôles à inventorier en priorité sont :

- gestion moteur diesel ;
- transmission automatique ;
- accès/démarrage et antidémarrage ;
- boîte de jonction et fonctions de carrosserie ;
- contrôle dynamique/freinage ;
- éclairage et ouvrants ;
- combiné d'instruments.

Cette liste n'affirme aucun nom de calculateur précis avant l'observation réelle.

## Signaux recherchés

La fiche contient les cibles sémantiques du contrôleur : tension batterie,
régime moteur, frein de service, frein de stationnement, type de transmission,
rapport, portes, coffre, verrouillage et défaut critique. Le capot peut rester
`non requis` lorsque `requireHoodClosed = false`.

Une valeur visible dans ISTA ou un autre outil reste une source diagnostic
candidate. Elle ne devient pas automatiquement un identifiant CAN ni un signal
`Verified` du profil.

## Résultat attendu

À la fin de cette phase, la fiche doit permettre de décider :

- quels calculateurs et variantes sont réellement présents ;
- quelles valeurs sont accessibles sans action dangereuse ;
- quels signaux devront être cherchés ensuite dans une capture brute ;
- quelles informations sont absentes et doivent rester `Unavailable` ;
- si une interface matérielle supplémentaire est nécessaire pour la phase CAN.
