# Backlog des idées de fonctionnalités

Ce document conserve les idées retirées du catalogue exécutable pendant la
Phase 1 de simplification. Elles ne sont **ni implémentées, ni promises, ni
disponibles dans la configuration utilisateur**.

Toute idée devra repasser par l'analyse de sécurité, l'observation CAN en
lecture seule, la validation sur plusieurs véhicules et une décision
d'architecture avant de pouvoir revenir dans le code. Les idées impliquant une
écriture véhicule ou une fonction antivol sont explicitement hors du périmètre
actuel.

## Sécurité et accès

- `passive_ble_access` — accès mains libres BLE
- `sequential_kill_switch` — kill-switch séquentiel
- `alarm_push_notification` — notification d'alarme
- `valet_mode` — mode valet
- `anti_carjacking` — anti-carjacking
- `slam_and_go_lock` — verrouillage « Slam & Go »
- `hands_free_trunk` — coffre mains libres
- `phone_left_behind_alert` — alerte téléphone oublié

## Performance, télémétrie et cockpit

- `needle_sweep` — balayage des aiguilles
- `external_shift_light` — shift-light externe
- `android_racing_dashboard` — tableau de bord racing Android
- `steering_wheel_m_mode` — mode M au volant
- `oil_temperature_gauge` — jauge de température d'huile
- `one_touch_dtc` — DTC en une touche
- `forced_dpf_regeneration` — régénération FAP forcée
- `active_transmission_cooling` — refroidissement actif de la BVA
- `flight_recorder` — enregistreur de données
- `virtual_obd_ble` — interface OBD2 BLE virtuelle
- `smart_turbo_timer` — turbo timer intelligent

## Éclairage et signalisation

- `custom_welcome_lighting` — éclairage d'accueil personnalisé
- `rapid_headlight_flash` — appel de phares rapide
- `four_corner_strobe` — stroboscope quatre coins
- `dynamic_cornering_lights` — éclairage dynamique de virage
- `directional_follow_me_home` — follow-me-home directionnel
- `dynamic_ambient_lighting` — éclairage d'ambiance dynamique

## Confort et automatismes

- `custom_startup_sequence` — séquence de démarrage personnalisée
- `automatic_defrost` — dégivrage automatique
- `dynamic_reverse_tilt` — inclinaison dynamique du rétroviseur
- `rain_window_closure` — fermeture des vitres sous la pluie
- `high_speed_window_closure` — fermeture des vitres à haute vitesse
- `reverse_audio_ducking` — baisse audio en marche arrière
- `drive_through_assistant` — assistant péage et drive
- `steering_wheel_remap` — remappage des boutons du volant
- `automatic_hotspot` — point d'accès automatique
- `smartphone_voice_assistant` — assistant vocal smartphone
- `multi_driver_profiles` — profils multi-conducteurs

## Règles et priorités logicielles

- `smoker_window_override` — exception vitres pour mode fumeur
- `rain_smoker_priority` — priorité pluie/mode fumeur
- `passenger_reverse_tilt_override` — exception rétroviseur avec passager
- `manual_override_priority` — priorité aux commandes manuelles
