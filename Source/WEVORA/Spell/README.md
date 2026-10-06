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

## 汎用魔法基盤（Fire / Wind）

責務: `Element × Shape × Delivery × Target State → Reaction`。

- Weaving: 既存のQ / LMB / E / GestureRecognizer / Cast / Cancelを維持。
  `AvailableElements`で切替順を設定。今回の属性はFire / Windのみ。
- Spell Data: `FWEVORASpellData`にElement / Shape / Delivery / Power / 世界座標Direction。
  Powerは初期値1の倍率。画面の入力方向・軌跡は従来のContextに残す。
- Cast: `ShapeProfiles`でThrust入力をForwardへ変換。Sweep / Circle / Slamも設定で変換。
  認識と配送を分けるため、Gestureを魔法スキル名として扱わない。
- Delivery: 新しい抽象Actor `AWEVORASpellDelivery`が生成時のLaunchを保持し、
  `DeliverToTarget`で対象へ渡す。既存`AWEVORASpellProjectile`はその派生で移動・衝突・寿命を担当。
- Reaction: 新しい`UWEVORASpellReactionComponent`を既存Enemyの標準Componentとして追加。
  属性、任意のShape、必要な対象Stateに一致した`ReactionRules`のEffectsを適用する。
  Projectile側は敵クラスやHPを判定しない。Damageは既存HealthComponentの標準Unreal Damage経路へ接続。

処理フロー:

`Input → GestureRecognizer → Weaving.OnSpellCast(Context)`
`→ Cast.ResolveSpell(ShapeProfiles / ElementParameters / Power)`
`→ Launch(Spell Data + 配送設定) → Delivery / Projectile → swept Hit`
`→ 対象のReaction Component → 一致RuleのEffects → HP / Burning / 物理作用`

### 今回の初期作用

| 属性 | Effects | 初期値 |
|---|---|---|
| Fire | Damage + Burning | 直接25HP、燃焼5HP/秒を3秒 |
| Wind | Knockback + Lift | 発射方向900cm/s、上方向220cm/s、直接ダメージなし |

- Fireの直接ダメージ直後はHP75、その後燃焼が終わるとHP60（他の被弾がない場合）。
  燃焼は約1秒間隔で処理し、終了時に残り時間分を精算。再度Fireが当たると持続時間を更新し、重複させない。
- Windは物理Bodyなら速度変化としてImpulse、CharacterならLaunchCharacterを使用。
  既存の浮遊敵と非物理Movable Actorは、Reaction側の外力速度を衝突sweep付きで減衰移動する。
  敵AIは外力移動中の追尾・射撃を休み、終了後に再開する。壁に当たれば外力移動を終了する。
  浮遊敵のLift後の高さはその後の既存AIが管理する。重力落下やVortexは今回対象外。
- 所有者への命中、既定で他プレイヤーへのReaction、生存HP0への作用を除外。
- 壁などReceiverを持たない対象では弾を消すだけ。世界のActorへ作用させるには
  `WEVORASpellReactionComponent`を追加する。静的な壁は移動せず、MovableまたはPhysics設定が必要。
- 燃焼／外力のない間はReaction ComponentのTickを停止。破棄時に状態と参照を解放する。
- Sweep / Circle / Slamは既存入力を壊さないため、**今回も1発のProjectileで配送する仮設定**。
  横薙ぎ、範囲攻撃、Fieldの実動作は未実装。Circleは半径1.5倍、Forwardは速度1.2倍。
  `DeliveryClasses`で専用の派生Actorへ差し替え可能。未登録のSweep / Area / Fieldを指定した場合は発射せずログ表示。

### Editor上の確認手順

検証マップ: `/Game/ThirdPerson/Lvl_ThirdPerson`。既存の`WEVORA_CombatEnemy`を利用する。

1. Editorを再起動して新しいC++クラスを読み込み、通常のGameModeでPlay。
2. Viewportをクリック。初期属性Fire。QでFire / Wind切替。
3. LMBを押したままEを押し、**マウスを上へ動かして**Eを離す。
   既存表示で`Thrust / ReadyToCast`を確認。Thrustが世界の`Forward`に対応する。
4. LMBは保持したまま敵を画面中央へ狙い直し、LMBを離してCast。
   地上でも空中でも可能。空中ならSpaceでジャンプ、左Altでホバー。
5. Fire: 球が敵へ命中しHP表示が減る。約3秒の燃焼でもHPが減る。
   敵の`HealthComponent / Show Damage Feedback`を有効にするとHPを画面で確認できる。
6. QでWindを選び同じ操作。命中時に敵が後方・上方へ動き、追尾・射撃が一時停止して再開する。
   Wind単体はHPを減らさない。Fire後は残っている燃焼でHPが減る点に注意。
7. E解放前のLMB解放はCancel。壁へ撃つと弾が消え、壁越しには作用しない。
8. Stop → Playで敵HPと状態をリセット。動きながらの狙いやすさ、押し出し量、Liftの高さは人間が判断する。

狙いやすさだけを見る場合は配置した敵のMove Speedを0、Attack Intervalを長めに調整できる。
Output Logの`LogWEVORA`でCast → Spell spawn → Spell hitを確認できる。
敵のReaction Componentの`BurningRemaining / ExternalVelocity`はPIE中にDetailsで参照可能。

### Blueprintで調整する主要値

| 場所 | 値 |
|---|---|
| Player / SpellWeavingComponent | AvailableElements、Gesture Thresholds |
| Player / SpellCastComponent | SpellPower、ElementParametersのSpeed / Radius / Lifetime / Color |
| Player / SpellCastComponent | ShapeProfilesのShape / Delivery / SpeedMultiplier / RadiusMultiplier |
| Player / SpellCastComponent | DeliveryClasses、既存ProjectileClass、AimDistance / SpawnDistance |
| Enemy / SpellReactionComponent | ReactionRulesのElement / bMatchShape / Shape / RequiredState / Effects |
| Enemy / SpellReactionComponent | EffectsのReaction / Magnitude / Duration、DisplacementDeceleration |
| Enemy / HealthComponent | MaxHealth / Show Damage Feedback |

以前の`ElementParameters.Damage`は責務分離により廃止し、対象の`ReactionRules → Damage.Magnitude`へ移した。
既存BlueprintでDamageを独自変更していた場合は、対象側で再設定する。
PowerはDamage / Burning DPS / Knockback / Liftの強さへ掛かり、Burningの時間には掛からない。
Reaction Ruleの照合は**着弾前の状態**で一括実行するため、Ruleの並び順によって条件が変わらない。
同じ条件の複数Ruleはすべて適用する。

`Wind → Burning`を試す場合は、Wind / RequiredState=BurningのRuleを追加し、
追加DamageやLiftを設定するだけ。大規模な合成や相性表は導入していない。
新属性はEnum、AvailableElements、属性配送プロフィール、表示プロフィール、対象Ruleへ追加する。
既存Projectile / Reactionの属性分岐を書き換える必要はない。
新しい作用の種類そのものを追加する場合は、Reaction Enumと作用実行処理を追加する。

### 確認範囲と次の実装

自動テストは保存済みCharacter / Enemy Blueprintを使い、公開入力APIからCast、World tick、
実際のProjectile sweep命中までを確認する。キーボード実入力・描画・プレイフィールの証明ではない。

- AirborneCastToDamage: Forward、Fire HP / Burning、Wind押し出し / Lift、属性スナップショット。
- CancelWallsAndLifetime: Cancel、壁、近接Spawn阻止、寿命、購読と自己衝突除外の解放。
- ReactionRulesAndStates: Wind × Burningを設定追加だけで検証、Shape条件、Power、燃焼終了。
- DisplacementWallsAndAI: 壁で止まる外力、敵AI再開、HPのない世界Actor / Physics Bodyへの作用。
- DataResolution: Gesture / Shape分離、未実装Deliveryの拒否、設定による属性選択。

最終UE 5.8 Mac Editorビルド成功、自動テスト14/14成功・テスト警告0/エラー0。
保存済み初期マップのEnemy Receiver / HP表示と、Player BlueprintのShapeProfiles継承を確認。
結果:`Saved/Automation/SpellFoundationFinal/index.json`、ログ:`Saved/Logs/SpellFoundationFinal.log`。

次は人間のPIE確認後、Sweep / AreaのDeliveryを1種類ずつ追加してShapeの違いを遊びへ反映する。
その後、燃焼中の対象を分かりやすくする仮表示と、Wind × Burningの具体的な作用を1Ruleだけ追加する。
正式VFX / UI、追加属性、ネットワーク同期は今回対象外。

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

## 今回変更したファイル

- 新規: `Spell/WEVORASpellDelivery.h/.cpp`、`Spell/WEVORASpellReactionComponent.h/.cpp`。
- 更新: `Spell/WEVORASpellTypes.h`、`Spell/WEVORASpellCastComponent.h/.cpp`、
  `Spell/WEVORASpellProjectile.h/.cpp`、`Spell/WEVORASpellWeavingComponent.h/.cpp`、
  `AI/WEVORAEnemy.h/.cpp`、`Tests/WEVORASpellCombatTests.cpp`（すべて`Source/WEVORA`内）。
- 文書: `Source/WEVORA/Spell/README.md`、`Source/WEVORA/AI/README.md`、`PROGRESS.md`。
- Map / Blueprintのアセットは変更せず、既存保存済みアセットへのネイティブComponent継承を検証。
