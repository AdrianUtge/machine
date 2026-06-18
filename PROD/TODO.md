# TODO — corrections à faire

> Liste des dettes/bugs connus à reprendre. Les éléments marqués 🔴 sont des
> contournements temporaires (hardcode) à retirer.

---

## 🔴 #1 — Jeton Bearer hardcodé dans le backend (À CORRIGER)

**Symptôme observé.** L'ESP recevait `Authorization: Bearer bearer_token_secret`
au lieu du vrai jeton (`Expected: Bearer 1276371237612hj1h12387dsads8912`) → 401.
La résolution du jeton depuis `.machine_config.ini` ne remonte pas correctement
jusqu'à l'appel WiFi dans l'environnement actuel.

**Contournement en place (temporaire).**
- Fichier : `backend/api.py`, fonction `connect()`.
- Le jeton est **hardcodé** :
  `key = "1276371237612hj1h12387dsads8912"  # TODO(config): dé-hardcoder`
- La ligne propre `key = nodeMcu_config.get('key', '')` est commentée juste au-dessus.
- (Note : le jeton est aussi présent dans `backend/machine_config.py` `_DEFAULTS`.)

**Ce qu'il faut faire pour corriger proprement.**
1. Diagnostiquer pourquoi `machine_config.nodemcu()['key']` ne contient pas la
   bonne valeur **au runtime** dans l'environnement de l'utilisateur :
   - vérifier le `.machine_config.ini` effectivement chargé :
     `cd backend && python3 -c "import machine_config as m; print(m.config_path()); print(m.nodemcu())"`
   - confirmer que le backend tourne bien depuis `PROD/backend/` (et pas une
     copie/ancienne arbo) → `_find_config_file()` doit remonter jusqu'à `PROD/.machine_config.ini` ;
   - vérifier qu'aucune **autre** `.machine_config.ini` n'est trouvée plus haut
     dans l'arborescence (la recherche s'arrête au 1er trouvé) ;
   - vérifier le **cache module** (`_parser`) : si `machine_config` est importé
     avant que le `.ini` soit en place, la 1ère lecture fige les défauts.
2. Une fois la cause trouvée : **retirer le hardcode** dans `api.py` (réactiver
   `key = nodeMcu_config.get('key', '')`) et **retirer le jeton de `_DEFAULTS`**
   dans `machine_config.py` (un défaut vide est plus sûr).
3. Retester de bout en bout : `[AUTH] Received` (ESP) doit matcher `[AUTH] Expected`.

**Pourquoi c'est important.** Le hardcode met un secret en clair dans le code
versionné — exactement ce que `.machine_config.ini` (gitignoré) doit éviter.

---

## Autres dettes connues (non bloquantes) — voir AUDIT_REPORT.md §8

- Firmware OpenRB en **ÉTAPE 1** : pas de boucle fermée de force (`SET_FORCE` ne
  fait que mémoriser la consigne).
- **Calibration force** (ADC→Newton : `FORCE_GAIN/OFFSET`) et **conversion
  mm↔Dynamixel** (`DXL_PER_MM`) = placeholders à régler avec le matériel.
- `STOP` ≡ `hard_reset` côté API (pas d'arrêt « doux » distinct).
- CORS ouvert (`*`) et mot de passe WiFi faible par défaut.
