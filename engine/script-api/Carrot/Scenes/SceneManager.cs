using System.Runtime.CompilerServices;

namespace Carrot {
    public class SceneManager {
        [MethodImpl(MethodImplOptions.InternalCall)]
        public static extern Scene GetMainScene();
        
        /**
         * Call 'clear' + 'deserialize' on the main scene.
         * Note: because this does not change viewport bindings, binding the main scene, and calling 'changeScene'
         *  will ensure the main scene is still drawn to its bound viewports
         */
        [MethodImpl(MethodImplOptions.InternalCall)]
        public static extern Scene ChangeScene(string vfsPath);
        
        /**
         * Loads the scene at the given path and add it to the scene list.
         * Scenes loaded via loadScene are completely independent.
         * Returns the new scene
         */
        [MethodImpl(MethodImplOptions.InternalCall)]
        public static extern Scene LoadScene(string vfsPath);
        
        /**
         * Loads scene data from the given path, and add it to the 'addTo' scene.
         */
        [MethodImpl(MethodImplOptions.InternalCall)]
        public static extern Scene LoadSceneAdditive(string vfsPath, Scene addTo);
        
        /**
         * Deletes the given scene from memory.
         * This call MUST be the last access to 'scene', because the SceneManager WILL delete the instance
         */
        [MethodImpl(MethodImplOptions.InternalCall)]
        public static extern void Delete(Scene toDelete);
    }
}
