using Carrot;
using System;
using Carrot.Input;

public class SceneSwitchSystem : LogicSystem {

    private BoolInputAction switchToScene1 = BoolInputAction.Create("switch_1");
    private BoolInputAction switchToScene2 = BoolInputAction.Create("switch_2");
    private BoolInputAction switchToScene3 = BoolInputAction.Create("switch_3");
    private BoolInputAction additiveLoad = BoolInputAction.Create("additive_load");
    private ActionSet inputSet = ActionSet.Create("inputs");

    public SceneSwitchSystem(ulong handle): base(handle) {
        switchToScene1.SuggestBinding("/user/glfw/keyboard/295"); // F6
        switchToScene2.SuggestBinding("/user/glfw/keyboard/296"); // F7
        switchToScene3.SuggestBinding("/user/glfw/keyboard/297"); // F8
        additiveLoad.SuggestBinding("/user/glfw/keyboard/298"); // F9
        
        inputSet.Add(switchToScene1);
        inputSet.Add(switchToScene2);
        inputSet.Add(switchToScene3);
        inputSet.Add(additiveLoad);
        inputSet.Activate();
    }

    public override void Tick(double deltaTime) {
        if (switchToScene1.WasJustReleased()) {
            SceneManager.ChangeScene("game://scenes/main.json");
        }
        if (switchToScene2.WasJustReleased()) {
            SceneManager.ChangeScene("game://scenes/second.json");
        }
        if (switchToScene3.WasJustReleased()) {
            SceneManager.ChangeScene("game://scenes/third.json");
        }
        if (additiveLoad.WasJustReleased()) {
            SceneManager.LoadSceneAdditive("game://scenes/third.json", SceneManager.GetMainScene());
        }
    }
}