const uint TASK_WORKGROUP_SIZE = 32;
const uint MESH_WORKGROUP_SIZE = 64;

struct VisibilityPayload
{
    uint clusterInstanceIDs[TASK_WORKGROUP_SIZE];
};