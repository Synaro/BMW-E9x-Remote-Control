# Inventaire diagnostic privé — {{SESSION_NAME}}

> Ne pas inscrire le VIN complet, l'immatriculation, une adresse, un nom ou une
> autre donnée personnelle dans ce document ou dans les noms de fichiers.

## Contexte

| Champ | Valeur |
|---|---|
| Date et heure | À compléter |
| Profil projet attendu | `bmw-e90-2009-n47d20c-automatic` |
| Modèle / année | BMW E90 / 2009 |
| Moteur | N47D20C |
| Transmission | Boîte automatique |
| État du contact | À compléter |
| État du moteur | À compléter |
| Outil et version | À compléter |
| Interface et pilote | K+DCAN / à compléter |
| Alimentation stabilisée | Oui / Non / Non applicable |

## Inventaire des calculateurs

Recopier les noms et variantes exactement comme affichés. Ne pas deviner une
adresse ou une version manquante.

| Rôle | Nom affiché | Adresse | Variante / version | Communication | Notes |
|---|---|---|---|---|---|
| Gestion moteur | À compléter | | | | |
| Transmission automatique | À compléter | | | | |
| Accès / démarrage | À compléter | | | | |
| Boîte de jonction / carrosserie | À compléter | | | | |
| Freinage / dynamique | À compléter | | | | |
| Éclairage / ouvrants | À compléter | | | | |
| Combiné d'instruments | À compléter | | | | |
| Autre | | | | | |

## Valeurs d'état candidates

Chaque ligne doit décrire une observation reproductible. Une valeur visible par
diagnostic n'est pas encore un signal CAN validé.

| Cible projet | Calculateur | Nom exact dans l'outil | État A | Valeur A | État B | Valeur B | Fraîcheur / cadence | Confiance |
|---|---|---|---|---|---|---|---|---|
| Tension batterie | | | Stable | | Stable | | | Candidate |
| Régime moteur | | | Moteur arrêté | | Moteur au ralenti, seulement si séance prévue | | | Candidate |
| Frein de service | | | Relâché | | Appuyé, véhicule immobilisé | | | Candidate |
| Frein de stationnement | | | Serré | | Desserré uniquement en conditions maîtrisées | | | Candidate |
| Type de transmission | | | Lecture statique | | Contrôle indépendant | | | Candidate |
| Rapport engagé | | | `P` | | Ne pas changer pendant la première séance | | | Candidate |
| Portes fermées | | | Fermées | | Une porte ouverte, moteur arrêté | | | Candidate |
| Coffre fermé | | | Fermé | | Ouvert, moteur arrêté | | | Candidate |
| Véhicule verrouillé | | | Déverrouillé | | Verrouillé sans personne à bord | | | Candidate |
| Défaut critique | | | Aucun défaut provoqué | | Observation uniquement | | | Candidate |
| Capot fermé | Non requis si option désactivée | | | | | | | Unavailable / Candidate |

## Erreurs de communication

| Heure relative | Calculateur / fonction | Message exact | Contexte | Reproductible |
|---|---|---|---|---|
| | | | | |

## Fichiers de preuve privés

| Fichier | Description | Données personnelles retirées |
|---|---|---|
| | | Oui / Non |

## Conclusion de séance

- Calculateurs confirmés :
- Valeurs reproductibles :
- Valeurs absentes :
- Points à répéter :
- Anomalies ou risques observés :
- Étape suivante autorisée :
