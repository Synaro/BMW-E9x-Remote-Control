# Télémétrie et alertes V1

## Périmètre actuel

Le premier moteur de télémétrie en lecture seule traite trois fonctionnalités :

- protection moteur froid ;
- indicateur de régénération FAP ;
- alerte de surchauffe de l'huile de boîte automatique.

Il reçoit des signaux sémantiques et produit uniquement un rapport et des
alertes. Il ne possède aucun port d'actionneur, n'écrit aucune trame et ne peut
pas modifier le comportement du contrôleur de démarrage distant.

## Entrées

Le rapport utilise le régime moteur, la température du liquide de
refroidissement, la température d'huile moteur, la température d'huile de boîte
et l'état de régénération FAP. Chaque valeur conserve une qualité explicite :
`fresh`, `stale` ou `unavailable`.

Une fonctionnalité demandée mais privée d'un signal frais retourne
`unavailable`. Elle ne fabrique pas une température ou un état FAP de
remplacement.

Les identifiants CAN de la BMW réelle ne sont toujours pas connus ni supposés.
Le cadre synthétique `0x1FFFFF02` est réservé au simulateur et ne doit jamais
être émis sur un véhicule.

## États et alertes

Chaque surveillance possède un état `disabled`, `unavailable`, `normal` ou
`active`. Les alertes sont produites uniquement lors d'une transition :

| Surveillance | Activation | Retour normal |
|---|---|---|
| moteur froid | `cold_engine_high_rpm` | `cold_engine_recovered` |
| régénération FAP | `dpf_regeneration_started` | `dpf_regeneration_stopped` |
| huile de boîte | `transmission_overheat` | `transmission_temperature_recovered` |

Une lecture répétée dans le même état ne répète donc pas l'alerte. La
température de boîte utilise une hystérésis configurable : avec un seuil à
110 °C et une hystérésis de 5 °C, l'alerte s'active à 110 °C et ne disparaît
qu'en dessous de 105 °C.

La protection moteur froid considère le moteur froid si au moins une
température moteur fraîche est sous le seuil configuré. Elle devient active
uniquement lorsque le régime dépasse également sa limite.

## Configuration

Les trois fonctions restent indépendantes et désactivées par défaut :

```ini
feature.cold_engine_guard=true
feature.dpf_regeneration_indicator=true
feature.transmission_overheat_alert=true

cold_engine_maximum_rpm=2200
engine_warm_temperature_c=75
transmission_overheat_temperature_c=110
temperature_alert_hysteresis_c=5
```

Les seuils sont bornés par le validateur commun et persistés dans le format V3.
Les anciennes configurations V1 et V2 sont migrées avec les valeurs par défaut.

## Essai graphique

Lancer :

```powershell
.\scripts\simulator-gui.ps1
```

Puis ouvrir le bac à sable, cocher les fonctions souhaitées, modifier les
températures, le régime ou l'état FAP et cliquer sur **Appliquer l'état**. Le
cartouche télémétrie affiche les états et les nouvelles alertes. Cette interface
reste entièrement locale et hors véhicule.
