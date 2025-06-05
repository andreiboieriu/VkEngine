Speed = 7
RotationSpeed = 45

---@return nil
function init()
    transform = getTransform()
    transform:Rotate(Vec3.new(0, 1, 0), 180.0)
end

---@return nil
function update()
    -- transform = getTransform()

    -- if (Input.GetKey(KeyCode.W)) then
    --     transform:Translate(transform.forward, Speed * Time.deltaTime)
    -- elseif (Input.GetKey(KeyCode.S)) then
    --     transform:Translate(transform.forward, -Speed * Time.deltaTime)
    -- elseif (Input.GetKey(KeyCode.A)) then
    --     transform:Translate(transform.right, Speed * Time.deltaTime)
    -- elseif (Input.GetKey(KeyCode.D)) then
    --     transform:Translate(transform.right, Speed * -Time.deltaTime)
    -- elseif (Input.GetKey(KeyCode.Q)) then
    --     transform:Rotate(transform.forward, RotationSpeed * Time.deltaTime)
    -- elseif (Input.GetKey(KeyCode.E)) then
    --     transform:Rotate(transform.forward, RotationSpeed * -Time.deltaTime)
    -- elseif (Input.GetKey(KeyCode.Z)) then
    --     transform:Rotate(Vec3.new(0, 1, 0), RotationSpeed * Time.deltaTime)
    -- elseif (Input.GetKey(KeyCode.C)) then
    --     transform:Rotate(Vec3.new(0, 1, 0), RotationSpeed * -Time.deltaTime)
    -- end

    -- cameraTransform = Camera.GetTransform()
    -- cameraTransform:SetPosition(transform.position - transform.forward * 10.0 + Vec3.new(0, 5, 0))
    -- cameraTransform:LookAt(transform.position)
end

---@return nil
function late_update()

end
