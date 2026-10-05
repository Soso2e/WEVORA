# Spell Weaving Prototype

操作: LMB押下 → Eを押しながらマウス入力 → E解放 → LMB解放でCast。
QでFire/Windを切り替えます。編み中・Shape確定後も変更でき、次回も属性を保持します。
Eを再度押すとShapeを作り直します。未確定状態でLMBを離すとキャンセルします。

## 構造

- Character: キーをコンポーネントへ直接Bindし、DoLookから同じ入力を転送。
- SpellWeavingComponent: 状態、属性、記録期間、Debug、Cast通知を管理。
- GestureRecognizer: 差分を逐次集計して簡易認識。移動APIやVFXへの依存はありません。
- SpellTypes: Blueprintで読めるContext、Enum、調整値。

BP_ThirdPersonCharacterのSpellWeavingComponentでThresholdsを調整できます。
入力単位は既存Look Actionの値です。GestureDirectionは画面の右を+X、上を+Y、
Magnitudeは軌跡長、DurationはE押下から解放までのゲーム内秒数です。
既存MouseLookのNegateを想定してbInvertLookY=trueにしています。
Look Mappingの反転を変更した場合はこの設定も合わせてください。
CircleのDirectionは始点から終点への方向なので、閉じた円ではゼロになり得ます。

bShowDebugで画面表示、bLogEventsでイベントログを無効にできます。
OnSpellCastをBlueprintでBindし、Contextを受け取って後続処理を追加できます。
Enhanced InputのStarted/Completed/Canceledからも公開APIを呼び出せます。
将来キーをActionへ移行する場合はCharacterの同じBindKeyを除去して二重入力を避けてください。
Cast通知はCharacterの`SpellCastComponent`が購読し、共通Projectileを発射します。
敵へのダメージ接続は実装済み。ネットワーク同期、正式な発射・着弾VFXは未実装です。

## 最小戦闘ループ（Fire / Wind）

検証マップ: `/Game/ThirdPerson/Lvl_ThirdPerson`（Editor起動時の既存マップ）。
開始地点の前方約11m、座標`(1100, 0, 452)`に既存の`BP_WEVORAEnemy`を1体保存済み。
World Outlinerのラベルは`WEVORA_CombatEnemy`。地形とLevel Blueprintは変更していません。

1. 新しいC++ Componentを読み込むためEditorを開き直し、`Lvl_ThirdPerson`でPlay。
2. Viewportをクリックして入力を受け付ける状態にする。初期属性はFire。QでWind/Fireを切替。
3. Spaceでジャンプ。必要ならSpace長押しで上昇、空中で左Altを押し続けると減速・静止ホバー。
   マナが尽きると落下するので、地上で回復して再試行する。
4. LMBを押したまま、Eを押しながらマウスを横へ動かす。Eを離す。
   仮デバッグ表示のGestureが`Sweep`、状態が`ReadyToCast`になることを確認。
5. **LMBはまだ離さず、Eを離した状態で狙い直す。** 敵を画面中央へ合わせてからLMBを離してCast。
   マウスで編むとカメラも動く既存仕様のため、この狙い直しを挟む。
6. プレイヤーの前方から球体のProjectileが飛び、敵に命中すると
   敵のActor名付き`HP: 75 / 100`が約2秒表示される。初期Damageは25なので4発で敵が消える。
7. QでWindへ切り替えて同じ操作を試す。Windも同じ経路で25ダメージ。
   敵を倒したらStop → Playで再配置される。死亡画面やリスポーンは未実装。
8. 未確定でLMBを離すとキャンセルし、発射しない。壁に撃つと球が消え、壁の向こうにはダメージを与えない。

敵は通常どおり追尾・射撃する。狙いやすさだけを先に確認したい場合は、PIEの前に配置した敵の
`Move Speed=0` / `Attack Interval`を長めに調整できる（本実装では既存の初期値を維持）。
操作感、動きながらの命中、カメラの狙いやすさ、球の視認性、難易度は人間側で確認する。

### 発射経路と調整

`Input / GestureRecognizer → SpellWeavingComponent / FWEVORASpellContext → OnSpellCast`
`→ SpellCastComponent::ResolveSpell → FWEVORASpellLaunch → SpellProjectile`
`→ swept OnHit → ApplyPointDamage → 既存HealthComponent → HP / OnDeath`

- `FWEVORASpellContext`の属性・Gesture・Magnitude・DurationをCastごとにコピー。
  画面空間のGestureDirectionと、発射用のWorld空間Directionは分離。
- カメラ中央の最初の衝突点へプレイヤーの視点位置から狙う。空中や横向き移動中も同じ処理。
  対象なしではカメラ前方100mを狙う。発射前のSphere sweepで近い壁を通り抜けて生成しない。
- Fire/Windとも`AWEVORASpellProjectile`。`UProjectileMovementComponent`で直進し、壁/対象への
  最初の衝突で消滅。所有者との衝突は双方向に除外し、寿命消滅時も除外設定を解除。
- 既存HealthComponentを持つ生存対象へPointDamage。所有者とプレイヤーにはダメージを与えない。
- `BP_ThirdPersonCharacter`の`SpellCastComponent / ElementParameters`で速度・Damage・Radius・Lifetime・Colorを調整。
  初期Fire速度2200cm/s、Wind2600cm/s、Damage25、Radius16cm、Lifetime4秒。
  全Gestureで1発の球。Thrustは速度1.2倍、Circleは半径1.5倍。
- `ResolveSpell`が最小の挙動決定箇所。今後のEnergy/Modifier/状況はContext/Launchとこの処理へ追加できる。
  強さは今は一定Damage。魔法のマナ消費、風の特殊効果、属性反応は実装しない。
- 仮表示はエンジン標準Sphereと属性色のPoint Lightのみ。Niagara/専用素材は不要。

### デバッグと自動確認

- Output Logを`LogWEVORA`で絞ると、既存Castの属性/認識結果、`Spell spawn`、
  `Spell hit`のActor/Applied Damage/HP、`Spell spawn blocked`を追跡できる。
- `SpellCastComponent / bLogEvents=false`でSpawn/Hitログを一括停止。
  編み側の`bLogEvents`/`bShowDebug`、HealthComponentの`bShowDamageFeedback`はそれぞれ独立に無効化可能。
- `WEVORA.Spell.Combat.AirborneCastToDamage`: 保存済みキャラクター/敵Blueprint、実World tick、
  空中ホバー中のFire/Wind認識→Cast→実sweep命中→HP減少、視点方向、スナップショット、自己衝突除外。
- `WEVORA.Spell.Combat.CancelWallsAndLifetime`: 不完全/無効Gesture、近接壁でのSpawn阻止、
  壁遮断、寿命、衝突除外の片付け、操作解除、終了時購読解除。
- 最終Editorビルド成功、Headless既存9件＋追加2件＝11/11成功・テスト警告0。
  自動確認は描画/キーボード/Viewportの証明ではない。Playerビルドも未確認。
  結果:`Saved/Automation/SpellCombat/index.json`、ログ:`Saved/Logs/SpellCombatAutomation.log`。
- `Scripts/place_combat_enemy.py`は既存敵がいない場合だけ、この既存マップへ1体追加して保存する。
  既存敵がいる場合は設定や配置を変えず、保存済みマップの再読込を確認する。

## 魔法選択エフェクト

Characterの`SpellSelectionEffectComponent`が、`OnSpellPresentationChanged`を購読して表示を管理します。
認識処理と見た目を分離し、Tickで状態を監視せず、選択・状態の変更時だけ更新します。

- Qで属性切替: 体から選択属性を`Selection Preview Duration`秒表示（初期値0.35秒、内部名IdlePreviewDuration）。編み中にも体から表示、連続切替で延長。
- LMB押下から編み中・Shape確定まで: 右手に現在の属性を継続表示。Qで右手の属性も更新。
- Cast、途中キャンセル、操作キャラクターの解除: 即時停止。
- EndPlay: タイマー、通知購読、生成した表示コンポーネントを解放。

`BP_ThirdPersonCharacter`のコンポーネントで次を設定します。

1. `ElementEffects`のFire／Windへ右手用のループする`System`とColorを指定。
   体の切替演出は`SelectionSystem`で別素材を指定できます。未指定なら`System`を短時間再生します。
2. 右手のボーン`hand_r`へ追従します。`HandSocketName`で別のボーン／ソケットへ変更可能。
   手元からの位置・向き・サイズは`HandOffset`で調整してください（初期値は手の原点）。
   BeginPlayで取り付けとHandOffsetを適用するため、以前のMesh基準の固定オフセットも置き換わります。
   ボーン／ソケットが存在しない場合は警告を出し、元の取り付け先を維持します。
   Attachment設定の変更はPIEを再開始して反映してください。
   体側の位置・向き・サイズはMesh相対の`BodyOffset`で調整します。右手側とは独立しています。
3. 仮の色付きPoint Lightは素材なしで周辺を照らします。`bEnablePreviewLight`で無効化、
   `PreviewLightIntensity`／`PreviewLightRadius`で調整できます。
4. 実行中に設定を変えた場合は`RefreshEffect`で再適用。

Niagara側で以下のUser Parameterを作成し、色やEmitterの挙動へ接続してください。
パラメータが存在しない素材でも入力やCast処理には影響しません。

| Parameter | Niagara型 | 値 |
|---|---|---|
| `User.SpellColor` | Linear Color | 属性プロフィールのColor |
| `User.SpellElement` | Int32 | Fire=0 / Wind=1 |
| `User.WeavingState` | Int32 | Idle=0 / Weaving=1 / Shaping=2 / ReadyToCast=3 |
| `User.SpellGesture` | Int32 | None=0 / Thrust=1 / Sweep=2 / Circle=3 / Slam=4 |

同じSystemを両属性に設定して色を変えることもできます。属性プロフィールがない場合は旧表示を停止します。
System未指定の場合は仮ライトのみ表示します。Niagara素材自体は今回作成していません。
専用サーバーでは表示を生成しません。通知・選択状態のネットワーク同期は今後の実装対象です。

## 確認

自動テスト: WEVORA.Spell.Gestures / WEVORA.Spell.StateFlow / WEVORA.Spell.SelectionEffects / WEVORA.Spell.HandAttachment。
PIEで4GestureとQ切替、途中キャンセル、繰り返しCastを確認してください。
移動は従来通りWASD / Space / Ctrl / Shift / Altで確認してください。
選択表示はQで体側が点灯・消灯し、編み中は右手に追従することを確認してください。
編み中のQで体側の短時間表示と右手側の属性更新が同時に行われること、Shapeやり直し、Cast・キャンセル時の停止も確認してください。
Niagara素材設定後はUser Parameterの反映と、繰り返し操作で表示が残らないこともPIEで確認してください。
