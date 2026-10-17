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

## Stage B: 状態とHP

`UWEVORALocalizedStatusComponent`を任意Actorに追加しProfileまたはbUsePrototypeProfileを設定する。Health/Reactionは別コンポーネント。
`ApplyStatus / RemoveStatus / HasStatus / GetRemaining / ResetStatuses / AdvanceStatuses`が共通API。`States`は表示用の読み取りsnapshot。
各状態はZone / Type / Element / Intensity / Accumulation / Remaining / DPS / Source / Instigator / AppliedAt / Policyを持つ。
Intensityは無次元、DPSは明示値。状態適用そのものはHPを減らさない。DoTはUnreal標準ApplyDamage経由で既存Healthへ送る。

| Policy | 同じZone・Typeへの再適用 |
|---|---|
| Replace | 新規値で置換 |
| Refresh | 強度・蓄積・DPS・残時間をそれぞれ最大値へ |
| Accumulate | 強度・蓄積・DPSを加算、残時間は最大値 |
| Extend | 残時間を加算、他の値は最大値 |

全PolicyはIntensity10 / Accumulation100 / Duration30秒 / DPS25の既定上限で制限する。Blueprintで調整可能。
複数状態のDoTは**最大DPSの包絡線**を期限で区切って積分する。例: 10DPS×1秒と5DPS×3秒の同時状態は20HP。
30/60/120fpsや4秒のhitchでも同じ総ダメージ。Sourceは最大DPSの状態を使い、同強度はZone/Typeの辞書順で決める。
HealthのOnDeath / OnResetで即時に状態を破棄しTickを停止する。EndPlayでも解放。新しいTimerは使わない。

## Stage C: 伝播と配送

FireのBurning効果は既存Reaction Rulesから取得し、Hitで解決した各Zoneへ適用する。Profile非対応対象は従来の全身Burningのまま。
局所対象は全身Burningを破棄してから処理する。未解決Hitは通常の直撃／Windを維持するが、局所にも全身にもBurningを追加しない。
Reaction.HasState(Burning)は局所状態を集約して返す。旧BurningRemainingは全身状態だけを表し、局所対象では0。

`UWEVORALocalReactionProfile` DataAssetのRulesでTrigger / RequiredStatus / OutputStatus / OutputElementを指定する。
Windは命中Zoneかその隣接ZoneにあるBurningをseedにできる。SeedRadius、EffectRadius、MaxEdgeDistance、MaxPathDistance、MaxHops、最低強度をすべて満たす経路だけを伝播する。
既定はseed半径110cm、作用半径240cm、辺110cm、経路220cm、2hop、最低強度0.1。
強度・DPS・蓄積はseed値×1.15×Wind Power（0〜5）×0.8^hop、残時間はseed残時間×1.3。元Zoneもrefreshする。
単純な全身一括適用はしない。各経路は同じZoneを再訪せず、hop数は実行時にも16以下へ制限する。

直接付与は直ちに可視、局所反応は`TG_PostUpdateWork`の`CommitReactions`でフレーム内入力を一括処理する。
全入力と全Ruleは同じ状態snapshotを読み、Zone/Typeごとの提案を最大値でmergeして一回だけ適用する。新しい出力を同じbatchのseedにしない。
同フレームのFire/Wind呼出順、重複Wind、Rule順による再帰的増幅を防ぐ。直接ダメージ／死亡は従来通り各Hitで確定するため、致死攻撃をまたぐ順序はゲーム上の順序を維持する。
明示的シミュレーションでは全入力後にCommitReactions、次にAdvanceStatusesを呼ぶ。ApplyStatusのReplace/Extend等を別々の操作として呼ぶ順序はAPI呼出者の責務。

Deliveryが対象ごとに一イベント一回を保証する。VolumeはComponentの重複を対象別Hit配列にまとめ、各Zoneに適用しつつ直撃／外力は一回だけ確定する。
Fieldの各pulseは新イベント。同じpulseの重複callbackは無視。Projectileの既存bHitConsumedも維持。判定後にImpactを通知する。
Volumeは各Zoneの位置で形の範囲とLOSを調べる。Actor原点が範囲外でも接触した手が範囲内なら局所的に適用できる。
局所入力は一batch128件まで。状態・経路・入力・DoTは死亡／ResetReactions／ResetHealth／EndPlayで破棄する。
