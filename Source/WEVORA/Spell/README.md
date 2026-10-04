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
ネットワーク同期、ダメージ、発射・着弾VFXは実装していません。

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
