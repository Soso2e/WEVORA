# Localized Elemental Reactions

基準: PR #8 `feature/combat-playground` / `6fb59f4`。本ブランチのStacked PRだけで実装する。PR #8は人間の実プレイ検証待ちで変更・マージしない。

## Stage A: 調査とZone

ROADMAP 23〜34、GAME_DESIGNの局所反応、CombatPlayground、Reaction/Health/Delivery/Projectile/Volume/Enemyおよび既存33テストを調査。
既存はReaction Rulesが直撃・全身Burning・Wind外力を確定し、HealthがUnreal標準Damageを受ける。Projectileは一命中、Volumeは対象ごとpulse一命中。AIはWind中休止。PlaygroundはReaction・HP・位置・timerと配送をresetする。

ZoneはFNameのゲームID。標準8Zone以外も追加可能。`UWEVORAZoneProfile` DataAssetでBone/Physics Body/Collider名、位置anchor、隣接グラフを設定する。
Resolver優先順位: 登録Bone → 登録Physics Body → Collider（Bone/BodyのないHitのみ）。存在しないZoneへの設定・未登録Bone・別ActorのComponentはNone。推測で別部位に割り振らない。
正式リグに依存せず、まず仮Colliderを使う。mGear等への移行ではProfileを差し替え、AnchorBoneとOffsetを調整する。

## 責務と互換性の方針

Hit → Resolve → Local Status → Reaction → Health → Presentation。状態とHPの更新はHasAuthorityで制限する。Replicationは実装しない。
Profileを持たない対象は既存全身反応を維持。局所対応対象ではBurningを局所コンポーネントだけが管理する。全身Burningと局所Burningの二重DoTを許可しない。
直撃とWind外力は既存Reaction Rulesを維持する。Lightningの新状態や水属性は追加しない。

同一フレームの局所入力はまとめて確定し、直接付与を反応前に解決する。伝播は共通snapshotを読み、結果をmergeしてから反映する。反応内で新たに生まれた状態を別反応のseedに使わない。
複数Zoneの燃焼DoTは各瞬間で最大強度の一本に集約する（合計しない）。異なる期限は区間積分し、フレーム時間に左右されない。

以降の段階でAPI、調整値、検証結果、PIE手順を追記する。
