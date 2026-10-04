# プレイヤーを狙う敵 Blueprint

UnityのPrefabに近い再利用単位はUnrealの **Blueprintクラス**。配置するのはそのインスタンスです。

## 配置

1. 新しいC++クラスを読み込むため、開いているUnreal Editorで作業を保存してから再起動。
2. Content Browserの `Content/WEVORA/AI/BP_WEVORAEnemy` をレベルへドラッグ。
3. 床から100cm以上離し、プレイヤーからおよそ1000cm、壁や他の敵と重ならない位置へ配置。
4. ThirdPersonの通常のGameModeでPIEを開始。

球体の敵が視線の通るプレイヤーを見つけ、空中を直進して近づきます。射程内では銃口の球が大きくなり、0.4秒後にプレイヤーの現在位置へ弾を発射します。発射後の弾は直進するので回避できます。壁に隠れると追尾・予備動作を停止します。NavMeshやStateTreeの設定は不要です。

## 設定

BlueprintのClass Defaultsまたは配置した敵のDetailsで調整できます（距離はcm、時間は秒）。

| 設定 | 初期値 | 意味 |
|---|---:|---|
| Detection Range | 3000 | プレイヤーを検出する距離 |
| Attack Range | 1500 | 発射できる距離 |
| Preferred Distance | 650 | 近づく目標距離 |
| Move Speed | 450 | 移動速度 |
| Windup Time | 0.4 | 発射前の予備動作 |
| Attack Interval | 1.5 | 発射後の待ち時間 |
| Attack Damage | 10 | 1発のダメージ |
| Projectile Speed | 1600 | 弾の速度 |
| Projectile Class | BP_WEVORAEnemyProjectile | 差し替え可能な弾Blueprint |

`Body`と`Muzzle`のメッシュ・マテリアルを差し替え、`OnAttackStarted` / `OnAttackFired`イベントへ音やNiagaraを追加できます。弾は5秒で消え、壁やPawnへの衝突でも消えます。ダメージを与える対象はプレイヤーです。

プレイヤーには`HealthComponent`を追加済み。HPは100から開始し、被弾すると画面に残HPを一時表示します。`OnHealthChanged` / `OnDeath`でUIや死亡処理を接続できます。HP0のプレイヤーは敵の標的から外れます。敵はHP0で消えます。

## 制限・PIE確認

- 初期実装はシングルプレイヤー向け。ネットワーク同期、死亡画面、リスポーン、魔法による敵へのダメージ接続は含みません。HP0後はPIEを再開始してください。
- 障害物を迂回する経路探索はありません。直進移動は衝突で止まり、視線が切れると追尾を停止します。
- 見た目はエンジン標準の球体を使う仮表示。描画・予備動作・入力・戦闘の難易度はPIE確認が必要です。
- PIEでは検出・接近・射撃・回避・HP減少、壁越しに発射しないこと、射程外/HP0/PIE終了時の停止を確認してください。
- `WEVORA.AI.TargetAndAttack`のHeadless自動テストは、プレイヤー検出、発射方向、クールダウン、実際の弾のsweep被弾、壁の遮断、検出距離、死亡、購読解除を確認します。

## アセットの再作成

`Scripts/create_enemy_blueprints.py`は専用の2つのBlueprintだけを作成し、既存アセットのClass Defaultsを上書きしません。Pythonプラグインはコマンド実行時だけ有効にします。

```powershell
& 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'C:\00_Project\Unreal\WEVORA\WEVORA.uproject' -run=pythonscript '-script=C:\00_Project\Unreal\WEVORA\Scripts\create_enemy_blueprints.py' '-EnablePlugins=PythonScriptPlugin,EditorScriptingUtilities' -unattended -nop4 -nosplash -NullRHI
```
