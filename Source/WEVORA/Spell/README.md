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
ネットワーク同期、ダメージ、VFXは実装していません。

## 確認

自動テスト: WEVORA.Spell.Gestures / WEVORA.Spell.StateFlow。
PIEで4GestureとQ切替、途中キャンセル、繰り返しCastを確認してください。
移動は従来通りWASD / Space / Ctrl / Shift / Altで確認してください。
