Speed = 7
RotationSpeed = 45

---@return nil
function init()
    transform = getTransform()
    transform:Rotate(Vec3.new(0, 1, 0), 180.0)
end

---@return nil
function update()
    transform = getTransform()

    transform:Translate(transform.right, Speed * Time.deltaTime)

    if (Input.GetKey(KeyCode.W)) then
        transform:Translate(transform.forward, Speed * Time.deltaTime)
    elseif (Input.GetKey(KeyCode.S)) then
        transform:Translate(transform.forward, -Speed * Time.deltaTime)
    elseif (Input.GetKey(KeyCode.A)) then
        transform:Translate(transform.right, Speed * Time.deltaTime)
    elseif (Input.GetKey(KeyCode.D)) then
        transform:Translate(transform.right, Speed * -Time.deltaTime)
    elseif (Input.GetKey(KeyCode.Q)) then
        transform:Rotate(transform.forward, RotationSpeed * Time.deltaTime)
    elseif (Input.GetKey(KeyCode.E)) then
        transform:Rotate(transform.forward, RotationSpeed * -Time.deltaTime)
    elseif (Input.GetKey(KeyCode.Z)) then
        transform:Rotate(Vec3.new(0, 1, 0), RotationSpeed * Time.deltaTime)
    elseif (Input.GetKey(KeyCode.C)) then
        transform:Rotate(Vec3.new(0, 1, 0), RotationSpeed * -Time.deltaTime)
    end
    -- print("Position in update: ", transform.position.x, transform.position.y, transform.position.z)
end

---@return nil
function late_update()
    transform = getTransform()
    -- print("Position in late update: ", transform.position.x, transform.position.y, transform.position.z)

    cameraTransform = Camera.GetTransform()

    cameraTransform:SetPosition(transform.position)
    -- cameraTransform:LookAt(transform.position)

    -- print("Camera position in late update: ", cameraTransform.position.x, cameraTransform.position.y,
    -- cameraTransform.position.z)
end
