# Protocole local du bac à sable

## Rôle et frontière de sécurité

Le mode `--sandbox` fournit une session persistante pour l'interface graphique
et les tests hôte. Il exécute le vrai contrôleur applicatif, le runtime et le
superviseur des actionneurs, mais remplace toutes les interfaces physiques par
des objets en mémoire.

Il n'ouvre aucun port série ou réseau, ne lit aucun bus véhicule et ne possède
aucun accès GPIO. Ce protocole n'est pas prévu comme une interface de commande
du futur boîtier.

## Transport

Le processus est lancé ainsi :

```powershell
.\build\bmw_remote_simulator.exe --sandbox
```

Une commande ASCII est envoyée par ligne sur l'entrée standard. Le processus
répond par exactement un objet JSON compact sur la sortie standard, puis vide
le tampon. La commande `quit` renvoie son dernier instantané avant de terminer.

Chaque réponse contient notamment :

- `ok` et `error` pour le résultat de la commande ;
- `state`, `fault` et `supervisor_fault` ;
- le temps monotone et l'état du timer ;
- les sorties simulées d'allumage et de démarreur ;
- l'état véhicule complet ;
- le dernier événement, les actions produites et la taille du journal.

## Commandes autorisées

| Commande | Effet |
|---|---|
| `status` | Retourne l'instantané courant sans modifier la session. |
| `new required` | Recrée une session sûre avec capot obligatoire. |
| `new optional` | Recrée une session sûre sans exiger de signal de capot. |
| `start` | Envoie une demande de démarrage et la première mise à jour véhicule. |
| `stop` | Envoie une demande d'arrêt distant. |
| `timer` | Avance jusqu'à l'échéance du timer actuellement armé. |
| `takeover` | Confirme la reprise conducteur. |
| `reset` | Tente de réarmer le superviseur puis le contrôleur. |
| `watchdog` | Suspend volontairement le heartbeat au-delà de sa limite. |
| `interlock on\|off` | Modifie l'autorisation matérielle synthétique. |
| `vehicle ...` | Applique atomiquement une ou plusieurs valeurs véhicule. |
| `quit` | Retourne l'instantané puis ferme le processus. |

Les champs acceptés par `vehicle` sont :

```text
rpm=0..8000
battery=9000..16000
doors=closed|open
hood=closed|open|unavailable
trunk=closed|open
brake=pressed|released
parking=applied|released
gear=park|neutral|reverse|drive
critical=on|off
```

Une ligne peut contenir plusieurs champs. Ils sont d'abord tous analysés dans
une copie de l'état ; une clé inconnue ou une valeur hors limites refuse la
commande entière, sans modification partielle.

## Temps et superviseur

Le temps n'avance que lorsqu'une échéance ou une panne de watchdog est injectée.
Pendant une avance normale, le bac à sable entretient le heartbeat toutes les
250 ms et vérifie le retour simulé des sorties. `watchdog` est la seule commande
qui saute volontairement cet entretien.

Une panne du superviseur sécurise d'abord les sorties, puis est transmise au
runtime sous forme d'`ActuatorFailure`. Cette propagation identique au chemin
d'intégration attendu évite qu'une maquette graphique puisse afficher un état
plus optimiste que le noyau.
