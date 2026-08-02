# Contrat d'intégration matérielle

Ce document définit les limites à respecter pour les futurs adaptateurs. Il ne
fournit pas de schéma de câblage et n'autorise pas un essai direct sur véhicule.

## VehicleGateway

L'adaptateur véhicule doit :

- demander ou assembler un instantané cohérent ;
- valider plage, provenance et fraîcheur de chaque signal ;
- publier `Fresh` uniquement après validation complète ;
- repasser un signal en `Stale` lorsque son délai propre est dépassé ;
- ne jamais inventer une valeur sûre lors d'une perte de communication ;
- isoler les identifiants de trames et les détails BMW dans l'infrastructure.

Les seuils de fraîcheur devront être documentés par signal et testés avec perte,
retard, duplication et ordre incorrect des messages.

## ActuatorPort

L'adaptateur d'actionneurs doit :

- conserver toutes les sorties inactives à la construction et au reset ;
- rendre `secureOutputs()` idempotent et prioritaire ;
- empêcher matériellement l'engagement simultané de combinaisons interdites ;
- confirmer l'application réelle d'une commande avant de retourner `true` ;
- retourner `false` au moindre doute afin de déclencher `Fault` ;
- éviter toute logique métier propre à une transition.

`releaseRemoteControl()` représente un transfert atomique vers un conducteur
déjà authentifié. Il ne doit jamais être assimilé à un simple arrêt des sorties :
il doit confirmer que le moteur reste sous une chaîne de commande légitime avant
de retourner `true`. Au moindre doute, il retourne `false`, ce qui déclenche la
mise en sécurité.

## Source de commande distante

Le futur adaptateur BMW doit publier une impulsion de verrouillage sémantique
uniquement après validation de sa provenance. Le noyau peut ensuite appliquer
`LockSequenceDetector` et produire une demande après trois impulsions valides.
Les identifiants CAN, compteurs, états CAS/JBE et règles anti-rejeu restent
strictement dans l'infrastructure et devront être qualifiés par variante.

De même, l'adaptateur ne produit `DriverTakeoverConfirmed` qu'après satisfaction
des preuves de reprise retenues. Une portière ouverte, seule, n'est jamais une
preuve suffisante.

## TimerPort

Une seule échéance d'état est active à la fois. L'implémentation doit gérer le
retour à zéro du compteur monotone, garantir une échéance non anticipée et
produire exactement un événement `TimerElapsed` par armement valide.

## UserSettingsStore

L'adaptateur de persistance doit charger et enregistrer `UserSettings` sans
interpréter les règles métier. Il doit prévoir une version de schéma, un contrôle
d'intégrité, une écriture atomique et une stratégie de récupération après
coupure d'alimentation. Après chargement, `validateUserSettings()` reste
obligatoire ; une lecture techniquement réussie ne rend pas les valeurs sûres.

Une interface utilisateur ne doit écrire que dans ce port. Elle ne modifie
jamais directement `ControllerConfig`, les sorties ou l'état courant du moteur.

## NotificationSink

La notification est informative et ne participe jamais à la sécurité. Sa panne
ne doit pas bloquer la mise en sécurité. Aucun secret de commande, identifiant
véhicule complet ou donnée personnelle ne doit être journalisé.

## Séquence de qualification

1. **Simulation logicielle** : couverture des transitions et injections de
   défauts.
2. **Tests unitaires des adaptateurs** : données enregistrées ou générateur de
   bus, aucune sortie de puissance.
3. **Banc basse puissance** : charges factices et oscilloscope.
4. **HIL** : modèle véhicule, coupures d'alimentation et communications
   dégradées.
5. **Banc véhicule immobilisé** : roues dégagées ou transmission mécaniquement
   sécurisée, arrêt d'urgence et opérateurs qualifiés.
6. **Essais contrôlés** : uniquement après revue des résultats et analyse de
   risques signée.

Le passage à l'étape suivante exige des critères d'acceptation mesurables et la
conservation des rapports d'essai.
