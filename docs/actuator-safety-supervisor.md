# Superviseur de sécurité des actionneurs

## Rôle

`ActuatorSafetySupervisor` enveloppe un futur pilote concret qui implémente
`ActuatorPort`. Il ajoute une dernière barrière logicielle entre les intentions
du `Runtime` et les sorties physiques, sans connaître les broches, les relais ou
le véhicule.

Le composant est utilisable dès maintenant sur PC avec un pilote simulé. Il ne
commande aucun matériel dans le firmware actuel.

```text
Runtime -> ActuatorSafetySupervisor -> ActuatorPort matériel futur -> sorties
                  ^          ^
                  |          +-- retours d'état des sorties
                  +-- heartbeat, horloge et autorisation matérielle lue
```

## Invariants

- la construction demande d'abord le relâchement du démarreur puis la
  sécurisation de toutes les sorties ;
- aucune sortie ne peut être activée avant un premier `heartbeat()` et un
  `poll()` ayant confirmé l'autorisation matérielle ;
- le démarreur exige l'allumage commandé et, par défaut, son retour d'état
  confirmé ;
- le démarreur possède une durée maximale indépendante de la machine d'état ;
- une perte d'autorisation, un heartbeat périmé, une séquence interdite ou un
  retour incohérent mémorise le premier défaut et appelle immédiatement la mise
  en sécurité ;
- un retour en arrière de l'horloge est refusé, tandis que le rebouclage normal
  du compteur 32 bits est accepté ;
- le réarmement sécurise de nouveau les sorties et exige, par défaut, un retour
  disponible confirmant qu'elles sont toutes inactives ;
- la configuration est bornée et aucune allocation dynamique n'est utilisée.

## Boucle d'intégration future

L'intégration embarquée devra respecter cette organisation :

1. une tâche de contrôle fournit le heartbeat avec une horloge monotone ;
2. une tâche de supervision indépendante lit l'autorisation de lancement et les
   retours électriques puis appelle régulièrement `poll()` ;
3. convertir tout défaut nouvellement observé en `InfrastructureFailure` pour
   que le contrôleur applicatif mémorise également l'incident ;
4. ne laisser le `Runtime` exécuter ses décisions au travers du superviseur que
   lorsque ce dernier reste sain.

Un appel réussi à `enableIgnition()` ne suffit pas à confirmer le circuit. Le
retour doit devenir cohérent avant la fin de `feedbackGraceMs`. Lorsque
`requireFeedback` vaut `true`, `engageStarter()` reste refusé jusqu'à cette
confirmation.

## Défauts mémorisés

| Défaut | Cause |
|---|---|
| `invalid_configuration` | durée nulle, excessive ou incohérente |
| `initialization_failure` | sorties impossibles à sécuriser au démarrage |
| `clock_regression` | horloge non monotone hors rebouclage normal |
| `watchdog_expired` | heartbeat absent ou trop ancien |
| `hardware_interlock_lost` | autorisation matérielle absente ou perdue |
| `starter_timeout` | durée maximale du démarreur dépassée |
| `command_sequence` | ordre d'activation interdit |
| `driver_failure` | pilote concret ayant refusé une commande |
| `feedback_unavailable` | retour obligatoire absent après le délai de grâce |
| `feedback_mismatch` | état électrique différent de l'état commandé |
| `safing_failure` | relâchement ou sécurisation non confirmé |

Les valeurs par défaut de développement sont 500 ms pour le heartbeat, 5 s
pour le démarreur et 100 ms pour le retour d'état. Elles ne sont ni qualifiées
pour une BMW ni exposées comme préférences utilisateur. Leur qualification
appartient à la conception matérielle et aux essais sur banc.

## Limite de sécurité essentielle

Ce superviseur est une défense logicielle supplémentaire. Il ne remplace pas :

- un watchdog matériel indépendant capable de désactiver les sorties si le
  microcontrôleur ou son ordonnanceur se bloque ;
- un interverrouillage électrique qui empêche physiquement une combinaison
  dangereuse ;
- des étages de puissance conçus pour l'automobile ;
- des retours électriques indépendants et plausibles ;
- les essais sur charges factices, HIL puis véhicule immobilisé.

En particulier, si tout le logiciel cesse de s'exécuter, `poll()` ne peut plus
constater l'absence de heartbeat. Le matériel devra donc retomber seul à l'état
sûr.
