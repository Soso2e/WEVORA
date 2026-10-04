# WEVORA Progress

最終更新: 2026-10-04

現在フェーズ: プレイヤーを狙う浮遊敵Blueprint（実装済み・未確認）

## 現在の状態

- 完了: `BP_WEVORAEnemy` / `BP_WEVORAEnemyProjectile`を`Content/WEVORA/AI`へ作成・保存・コンパイル。浮遊追尾、距離/視線による検出、予備動作、照準射撃、クールダウン、弾の壁衝突・寿命を実装。
- 完了: 再利用可能なHealthComponentを敵とWEVORACharacterへ組込。標準UnrealダメージによるHP減少、被弾時の仮HP表示、HP0の標的除外、敵の消滅、Blueprintイベントを追加。
- 完了: UE 5.8 Editorビルド成功。Headless自動テスト4件（敵AIと既存魔法3件）成功。敵AIは保存したBlueprintの読込・実移動・検出・照準・射撃間隔・実sweep衝突によるHP減少・壁遮断・検出範囲・死亡・購読解除を確認。弾は視線/カメラのTraceを妨げない設定。
- 未確認: 敵BlueprintのPIEでの描画・追尾・回避・難易度、Playerビルド。開いているEditorは作業保存後に再起動して新クラスを読み込むこと。
- 制限: 敵は直進追尾で経路探索なし。シングルプレイヤー向け。死亡画面/リスポーン/魔法から敵へのダメージ接続/ネットワーク同期は未実装。
- 完了: 魔法編みの既存状態・属性・Cast処理を調査。選択/状態通知と表示専用コンポーネントを追加。
- 完了: Characterへ自動組込。Fire/Windの色付き仮ライト、属性別Niagara設定、Idleの短時間プレビュー、編み中の継続表示、Cast/キャンセル/EndPlay時の停止を実装。
- 完了: UE 5.8 WEVORAEditor Win64 Developmentビルド成功。
- 完了: Headless（NullRHI）自動テスト3件（Gestures / StateFlow / SelectionEffects）成功。素材なしの属性色、Idleタイマー、編み中継続表示、Cast/キャンセル、欠落プロフィール、EndPlay購読解除を確認。git diff --check成功。
- 作業中: なし。敵の体験確認と魔法の視覚調整はPIE確認待ち。
- 未確認: PIEでの入力と見た目、ライト位置・明るさ、実Niagara素材でのパラメータ反映、Playerビルド。
- 問題: 最初のビルドで既存DoRecenterViewのController変数が親クラスメンバーを隠すC4458が発生。ViewControllerへ改名後ビルド成功。
- 問題: 自動テストの初回・2回目はEndPlay購読解除の検証が失敗。テスト用WorldのActor初期化が不足していたため、初期化と終了のライフサイクルを明示して再実行し、3回目は全件成功。3回目のWorld context警告もテスト側の不要なActor Destroyを除去して解消し、最終4回目は3件成功・テスト警告0・失敗0。現在の自動確認に未解決の失敗なし。
- TODO: Fire/Wind用のNiagara素材を制作・設定し、PIEで確認。
- 次にやること: Editorを再起動し、BP_WEVORAEnemyをレベルへ配置してPIE確認。続けてBP_ThirdPersonCharacterのSpellSelectionEffectComponentでElementEffectsと表示Transformを調整。

## 確認方法

- 敵の配置/設定/確認手順: `Source/WEVORA/AI/README.md`。
- 敵を含む自動テスト: UnrealEditor-Cmd.exe <WEVORA.uproject> -unattended -nop4 -nosplash -NullRHI -ExecCmds="Automation RunTests WEVORA." -TestExit="Automation Test Queue Empty"。
- 敵テスト結果: `Saved/Automation/EnemyAI/index.json`、ログ: `Saved/Logs/EnemyAutomation.log`。
- ビルド: UE_5.8/Engine/Build/BatchFiles/Build.bat WEVORAEditor Win64 Development -Project=<WEVORA.uprojectの絶対パス> -WaitMutex -NoHotReloadFromIDE。
- 自動テスト: UnrealEditor-Cmd.exe <WEVORA.uproject> -unattended -nop4 -nosplash -NullRHI -ExecCmds="Automation RunTests WEVORA.Spell" -TestExit="Automation Test Queue Empty"。
- テスト結果: `Saved/Automation/SpellSelection/index.json`、ログ: `Saved/Logs/SpellSelectionAutomation.log`。
- PIE: IdleでQを押して属性色が短時間表示され消える。LMB中は表示が継続し、Qで切替。Eで形を作り直しても表示は継続。Cast・途中キャンセルで消える。
- 繰り返し操作、Possess解除、PIE終了で旧表示が残らないことを確認。
- Niagara設定後: READMEの4つのUser Parameterを素材側へ接続し、属性/形/状態の反映を確認。
- WASD / Space / Ctrl / Shift / Altの既存移動、視点リセットもPIEで確認。

## 設計メモ

- Git運用: ユーザーより必要な区切りでのコミットを許可（2026-10-04）。関連変更だけを対象とする。Push/PRは別途指示に従う。
- 認識・Cast処理と表示を分離し、OnSpellPresentationChangedで状態とContextを通知。表示のための毎フレーム監視は行わない。
- ElementEffectsは属性Enumをキーとする設定。System未指定時は仮ライトのみ、プロフィール未指定時は旧表示を停止。
- Niagaraコンポーネントと仮ライトは使い回し、EndPlayで解放。専用サーバーには生成しない。
- 魔法のNiagara素材、発射/着弾演出、魔法ダメージ、ネットワーク同期は未実装。敵の射撃ダメージはHealthComponentへ実装済み。
- プロジェクト内のAGENTS.md/Concept.md/既存PROGRESS.mdは見つからず。Spell/README.mdを参照。BRAINの20_Projects/30_Decisionsの対象語検索では関連ノートなし。

## 変更履歴

| ID | 内容 | 結果 | 変更ファイル | 次 |
|---|---|---|---|---|
| 2026-10-04-01 | 魔法選択エフェクト基盤、変更通知、Niagara依存、仮ライト、設定手順、ライフサイクルテストを追加。既存C4458を変数改名で修正。Editorビルド成功。 | 実装済み・未確認 | `Source/WEVORA/Spell/WEVORASpellSelectionEffectComponent.h/.cpp`, `Source/WEVORA/Spell/WEVORASpellWeavingComponent.h/.cpp`, `Source/WEVORA/Spell/README.md`, `Source/WEVORA/WEVORACharacter.h/.cpp`, `Source/WEVORA/WEVORA.Build.cs`, `Source/WEVORA/Tests/WEVORASpellWeavingTests.cpp`, `WEVORA.uproject`, `PROGRESS.md` | 自動テスト結果記録、Niagara素材設定、PIE確認 |
| 2026-10-04-02 | テスト用Actorの初期化・終了手順を修正。Editor再ビルド成功、Headless自動テスト3/3成功、diffチェック成功。描画・実入力は対象外。 | 実装済み・未確認 | `Source/WEVORA/Tests/WEVORASpellWeavingTests.cpp`, `PROGRESS.md` | Niagara素材設定、PIE確認、Playerビルド |
| 2026-10-04-03 | コミット・Push依頼に伴い変更範囲、現在ブランチfeature/spell-weaving-prototype、origin、diffチェックを確認。今回の実装・テスト・文書11ファイルをコミット対象とした。 | 調査完了 | 今回の関連11ファイル | コミット・Push、リモートHEAD一致確認 |
| 2026-10-04-04 | 浮遊敵と弾のC++/配置用Blueprint、HealthComponent、生成スクリプト、配置手順、自動テストを追加。Editorビルド成功・Headless 4/4成功。初回のdebug表示キー型のビルドエラーを修正。初回AIテストのGameMode不在によるPlayerState/被弾初期化不足をテストWorldの初期化で修正。BRAINの対象プロジェクト名ファイル検索で関連ノートなし。 | 実装済み・未確認 | `Source/WEVORA/AI/*`, `Source/WEVORA/WEVORACharacter.h/.cpp`, `Source/WEVORA/Tests/WEVORAEnemyTests.cpp`, `Content/WEVORA/AI/*.uasset`, `Scripts/create_enemy_blueprints.py`, `PROGRESS.md` | Editor再起動、配置とPIE確認、Playerビルド |
| 2026-10-04-05 | 必要な区切りでのコミット許可を記録。敵AIの関連14ファイルをコミット対象として確認。前回Editorビルド成功・Headless 4/4成功の記録を確認。 | 調査完了 | 敵AIの関連14ファイル | コミット、PIE確認 |
