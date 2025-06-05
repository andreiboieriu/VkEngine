Speed = 0.7
RotationSpeed = 45

Rotate = true
Timeout = 1.0

function init()
end

---@return nil
function update()
    transform = getTransform()

    if (Timeout > 0) then
        Timeout = Timeout - Time.deltaTime
    end

    if (Rotate) then
        transform:Rotate(Vec3.new(0, 1, 0), RotationSpeed * Time.deltaTime)
    end

    if (Input.GetKey(KeyCode.Space) and Timeout <= 0) then
        Rotate = not Rotate
        Timeout = 1.0
    end
end
